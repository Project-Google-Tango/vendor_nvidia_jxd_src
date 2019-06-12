/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.NvWFDSvc;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.content.Context;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.net.wifi.WifiManager;
import android.net.wifi.p2p.WifiP2pManager;
import java.util.ArrayList;
import java.util.List;
import com.nvidia.nvwfd.*;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.FileNotFoundException;
import java.util.Set;
import java.lang.String;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Collections;
import java.lang.reflect.Method;

import android.net.wifi.p2p.WifiP2pGroup;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.net.wifi.p2p.WifiP2pManager.Channel;
import android.net.wifi.p2p.WifiP2pManager.PersistentGroupInfoListener;
import android.net.wifi.p2p.WifiP2pGroupList;
import android.net.wifi.p2p.WifiP2pDevice;
import android.content.ContentResolver;
import android.location.LocationManager;
import android.provider.Settings.Secure;
import android.os.UserHandle;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.SystemProperties;
import android.net.NetworkInfo;

public class NvwfdService extends Service
        implements PersistentGroupInfoListener {

    private Looper mServiceLooper;
    private ServiceHandler mServiceHandler;
    private Context mContext;
    private Intent mMiracastSettingsIntent;
    private NotificationManager mNotificationManager;
    private static final String TAG = "NvWFDSvc";
    private boolean DBG = false;
    private Nvwfd mNvwfd;
    private List<NvwfdSinkInfo> mSinks;
    private NvwfdConnection mConnection;
    private NvwfdSinkInfo mConnectedSink;
    private String mPendingConnectedSink;
    private String mPendingAuth;
    private NvwfdPolicyManager mNvwfdPolicyManager;
    private boolean mInit = false;
    private NvwfdListener mNvwfdListener;
    private NvwfdServiceStateListener mServiceStateListener;
    private static final int SINKS_FOUND_MSG = 1;
    private static final int CONNECT_DONE_MSG = 2;
    private static final int DISCONNECT_START_MSG = 3;
    private static final int DISCOVERY_STATUS_MSG = 4;
    private static final int DISCONNECT_DONE_MSG = 5;
    private static final int PENDING_CONNECT_MSG = 6;
    private static final int SERVICE_DIED = 7;
    private static final int SERVICE_RESTARTED = 8;
    private static final int CANCEL_CONNECTION_MSG = 9;
    private static final int HDCP_AUTH_FAILED = 10;
    private static final int PAUSE_MSG = 11;
    private static final int RESUME_MSG = 12;
    private static long mDiscoveryTs;
    private static long mStartConnectionTs;
    private static long mDisconnectionTs;
    private String connectedSinkId = null;
    private int mPolicySeekBarValue;
    private boolean bForceResolution = false;

    private int mResolutionCheckId = NvwfdPolicyManager.CONNECTION_720P_30FPS;
    private boolean isDisconncetPressed = true;
    private HashMap<String, String> mSinkNickName;
    private HashMap<String, Integer> mSinkFrequency;
    private HashMap<Integer, INvWFDServiceListener> mClients;
    private HashMap<String, String> mGroupId;
    private boolean isDiscoveryInitiated = false;
    private static Object mClientsLock;
    private int connIdProvider = 100; // -1 indiacate the failure of the call
    private WifiP2pManager mWifiP2pManager;
    private WifiP2pManager.Channel mChannel;
    private static Object mSinksLock;
    private static final int PENDINGCONNECTION_TIMEOUT = 60000;
    private static Object mHistoryDataLock;
    private String mDisconnectedSink;
    private final IntentFilter mIntentFilter = new IntentFilter();
    private List<String> mBusySinksList;
    private static final String MIRACAST_DISCONNECT_ACTION = "com.nvwfd.action.DISCONNECT";
    private static final String MIRACAST_FREQ_CONFLICT_ACTION = "com.nvwfd.action.FREQ_CONFLICT_DISCONNECT";
    private static final String PREFS_NVWFD_UI_GAME_MODE = "prefs_nvwfd_ui_game_mode";
    private static boolean bHDMIActiveAtConnect = false;
    private static boolean mGameModeOption = false;
    private boolean mIsUserDisconnected = false;
    private static final int NOTIF_WARNING_CONN_FAILED = 0;
    private static final int NOTIF_WARNING_CONN_LOST = 1;
    private static final int NOTIF_WARNING_AUTHENTIFICATION_FAILED = 2;
    private static final int NOTIF_SINK_SEARCH = 3;
    private static final int NOTIF_CONNECTION_PROGRESS = 4;
    private static final int NOTIF_STREAMING_ON = 5;
    private int mPrevNotifId = -1;
    private int mWifiP2PState = WifiP2pManager.WIFI_P2P_STATE_DISABLED;
    private List<WifiP2pGroup> mWifiP2pGroups;
    private static final boolean DEFAULT_GAME_MODE_VALUE = true;

    private void setForeGround(boolean isForeground, Notification notif, int notificationId) {
        if (isForeground) {
            if (notif != null) {
                startForeground(notificationId, notif);
            }
        } else {
            stopForeground(true);
        }
    }

    private void initialize() {
        if (DBG) Log.d(TAG, "Initialize +");
        if (mInit) {
            onSetupComplete();
        } else {
            mWifiP2pManager = (WifiP2pManager) mContext.getSystemService(mContext.WIFI_P2P_SERVICE);
            if (mWifiP2pManager != null) {
                mChannel = mWifiP2pManager.initialize(mContext, mContext.getMainLooper(), null);
                if (mChannel != null) {
                    if (DBG) Log.d(TAG, "calling requestPersistentGroupInfo: ");
                    mWifiP2pManager.requestPersistentGroupInfo(mChannel, NvwfdService.this);
                }
            }
            mContext.registerReceiver(mReceiver, mIntentFilter);
            mInit = mNvwfd.registerServiceStateListener(mServiceStateListener);
            if (mInit == true) {
                mInit = mNvwfd.init(mContext, mNvwfdListener);
            }
        }
        if (DBG) Log.d(TAG, "Initialize -");
    }

    private void deInitialize() {
        if (DBG) Log.d(TAG, "deInitialize +");
        String sigmaProp = null;
        try {
            Class clazz = null;
            clazz = Class.forName("android.os.SystemProperties");
            Method method = clazz.getDeclaredMethod("get", String.class);
            sigmaProp = (String)method.invoke(null, "nvwfd.sigma.connectionactive");
            if (DBG) Log.d(TAG, "sigmaProp is: " + sigmaProp);

            if (mInit && !(mBinder.isConnected()) && mClients.size() == 0 && !sigmaProp.equalsIgnoreCase("1")) {
                mContext.unregisterReceiver(mReceiver);
                mWifiP2pManager = null;
                mChannel = null;
                mNvwfd.stopSinkDiscovery();
                if (DBG) Log.d(TAG, "deInitialize ,,,");
                mNvwfd.deinit();
                mInit = false;
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        if (DBG) Log.d(TAG, "deInitialize -");
    }

   /**
     * Class for clients to access.
     */
    //binder object receives interactions from clients.
    private final INvWFDRemoteService.Stub mBinder = new INvWFDRemoteService.Stub() {

        public int createConnection(INvWFDServiceListener callback) {
            // putting it here to avoid using persistent property
            if(Integer.parseInt(android.os.SystemProperties.get("nvwfd.debug", "0")) == 1) {
                DBG = true;
            } else {
                DBG = false;
            }
            readRememberedInfoFromFile();

            synchronized(mClientsLock) {
                mClients.put(new Integer(++connIdProvider), callback);
                try {
                     mServiceHandler.post(new Runnable() {
                        public void run() {
                            initialize();
                        }
                    });
                 } catch (Exception e) {
                    Log.e(TAG, "create connection failed " + e.toString());
                    return -1;
                }
                return connIdProvider;
            }
        }

        public void closeConnection(int connId) {
            synchronized(mClientsLock) {
                if (mClients.containsKey(new Integer(connId))) {
                    mClients.remove(new Integer(connId));
                } else {
                    Log.e(TAG, "couldn't find the connection id while closing conn !!!! ");
                }

                if (mClients.size() == 0) {
                    connIdProvider = 100;
                }
                updateRememberedSinks();

                try {
                    if (!isConnected()) {
                        mServiceHandler.post(new Runnable() {
                            public void run() {
                                deInitialize();
                            }
                        });
                    }
                } catch (Exception e) {
                    Log.e(TAG, e.getMessage());
                }
            }
        }

        public void discoverSinks() {
            if (mSinkNickName.size() != 0 || isConnected()) {
                if (DBG) Log.d(TAG, "mSinks size : " + mSinks.size());
                /* inform about the remembered sinks before the discovered sinks shown */
                mServiceHandler.sendEmptyMessage(SINKS_FOUND_MSG);
            }
            if (isConnected()) {
                 /* since discovery is started and coonection to remote display is already made
                 *  client will keep receiving sinks found call once bound and callback is registered
                 */
                if (DBG) Log.d(TAG, "discovery won't happen, as connection is already established");
                return;
            }

            synchronized(mSinksLock) {
               mSinks.clear();
            }
            // qos discover time start
            if (DBG) Log.d(TAG, "real discovery started *************");
            mDiscoveryTs = System.currentTimeMillis();
            mNvwfd.discoverSinks();
            isDiscoveryInitiated = true;
        }

        public void stopSinkDiscovery() {
            if(!isConnectionOngoing())
                mNvwfd.stopSinkDiscovery();
        }

        public void disconnect() {
            if (mConnectedSink == null) return;
            if (DBG) Log.d(TAG, "disconnect+ : " + mConnectedSink.getSsid());

            mIsUserDisconnected = true;
            mDisconnectionTs = System.currentTimeMillis();
            Message msg = new Message();
            msg.what = DISCONNECT_START_MSG;
            msg.obj = null;
            mServiceHandler.sendMessage(msg);
        }

        public String getConnectedSinkId() {
            String sinkId = null;
            if (connectedSinkId != null) {
                sinkId = mSinkNickName.get(connectedSinkId);
                // Nickname is removed from list using forget option from settings page, return the sink ssid
                if (sinkId == null) {
                    sinkId = connectedSinkId;
                }
            } else if (mConnectedSink != null) {
                // While connection is happening, return the connecting sink nick name or ssid for different clients
                sinkId = mSinkNickName.get(mConnectedSink.getSsid());
                if (sinkId == null) {
                    sinkId = mConnectedSink.getSsid();
                }
            } else if (mPendingConnectedSink != null) {
                // While connection is pending, return the pending connection sink nick name or ssid for different clients
                sinkId = mSinkNickName.get(mPendingConnectedSink);
                if (sinkId == null) {
                    sinkId = mPendingConnectedSink;
                }
            }
            return sinkId;
        }

        public boolean isConnected() {
            // return false if we are being connected to one of the client as there can only exists one connection
            return ((mConnection != null) || (mConnectedSink != null) || (mPendingConnectedSink != null) ? true : false);
        }

        public boolean isConnectionOngoing() {
            if (isConnected() && mConnection == null) {
                return true;
            }
            return false;
        }

        public boolean isInitialized() {
            return mInit;
        }

        public boolean connect(String sinkSSID, String mAuthentication) {
            if (DBG) Log.d(TAG, "connect " + sinkSSID);
            boolean result = false;
            // user might try to connect without checking current connection state as allow only one
            if((mConnectedSink != null) || (mPendingConnectedSink != null)) {
                if (DBG) Log.d(TAG, "we are already connected(or trying to connect) ");
                // if return value indicaes fail, don't expect any success/failure callback
                return false;
            }

            if (IsHDMIConnected()) {
                bHDMIActiveAtConnect = true;
            }
            else {
                bHDMIActiveAtConnect = false;
            }

            mPendingConnectedSink = null;
            mPendingAuth = null;
            mDisconnectedSink = null;
            // From power menu nickname is used to connect, retrive ssid from histroy data to connect
            Set s = mSinkNickName.entrySet();
            Iterator it = s.iterator();
            if (DBG) Log.d(TAG, "in connect, remembered set size:" + s.size());
            String sinkId = null;
            while (it.hasNext()) {
                Map.Entry m = (Map.Entry)it.next();
                if (DBG) Log.d(TAG, "nickName:" + m.getValue());
                sinkId = (String)m.getValue();
                if (sinkId != null && sinkId.equalsIgnoreCase(sinkSSID)) {
                    sinkSSID = (String)m.getKey();
                    break;
                }
            }

            String sinkName = null;
            synchronized(mSinksLock) {
                for (final NvwfdSinkInfo sink : mSinks) {
                    sinkName = sink.getSsid();
                    if (DBG) Log.d(TAG, "connectingSinkId " + sinkName);
                    if (sinkName != null && sinkName.equalsIgnoreCase(sinkSSID)) {
                        // Attempt connection
                        if (DBG) Log.d(TAG, "Attempting connect to " + sinkName);
                        mStartConnectionTs = System.currentTimeMillis();
                        mConnectedSink = sink;
                        mNvwfd.connect(sink, (mAuthentication==null) ? false:true,
                                    mNvwfdPolicyManager.getDefaultVideoFormat(mGameModeOption), null);
                        displayMiracastStatusNotification(NOTIF_CONNECTION_PROGRESS, getConnectedSinkId(), false, true);
                        result = true;
                    }
                }
            }

            if (!result && isDiscoveryInitiated) {
                if (DBG) Log.d(TAG, "delay connect :" + isDiscoveryInitiated);
                mPendingConnectedSink = sinkSSID;
                mPendingAuth = mAuthentication;
                Message msg = new Message();
                msg.what = PENDING_CONNECT_MSG;
                mServiceHandler.sendMessageDelayed(msg, PENDINGCONNECTION_TIMEOUT);
                displayMiracastStatusNotification(NOTIF_SINK_SEARCH, getConnectedSinkId(), false, true);
                result = true;
            } else if (!isConnectionOngoing()) {
                if (sinkId != null) {
                    displayMiracastStatusNotification(NOTIF_WARNING_CONN_FAILED, sinkId, true, true);
                } else {
                    displayMiracastStatusNotification(NOTIF_WARNING_CONN_FAILED, sinkSSID, true, true);
                }
                if (DBG) Log.d(TAG, "Sink not available. Discover sinks again.");
            }
            return result;
        }

        public List<String> getSinkList() {
            List<String> mSinkList = new ArrayList<String>();
            mBusySinksList.clear();
            synchronized(mSinksLock) {
                for (final NvwfdSinkInfo sink : mSinks) {
                    mSinkList.add(sink.getSsid());
                    if (sink.isBusy()) {
                        mBusySinksList.add(sink.getSsid());
                    }
                }
            }
            return mSinkList;
        }

        public boolean getPbcModeSupport(String sinkSSID) {
            synchronized(mSinksLock) {
                for (final NvwfdSinkInfo sink : mSinks) {
                    if (sink.getSsid().equals(sinkSSID)) {
                        return sink.isPbcModeSupported();
                    }
                }
            }
            return false;
        }

        public boolean getPinModeSupport(String sinkSSID) {
            synchronized(mSinksLock) {
                for (final NvwfdSinkInfo sink : mSinks) {
                    if (sink.getSsid().equals(sinkSSID)) {
                        return sink.isPinModeSupported();
                    }
                }
            }
            return false;
        }

        public boolean getSinkBusyStatus(String sinkSSID) {
            synchronized(mSinksLock) {
                for (final NvwfdSinkInfo sink : mSinks) {
                    if (sink.getSsid().equals(sinkSSID)) {
                        return sink.isBusy();
                    }
                }
            }
            return false;
        }

        public List<String> getSupportedAudioFormats() {
            List<String> audioFormats = null;
            if (mConnection != null) {
                audioFormats = new ArrayList<String>();
                List<NvwfdAudioFormat> audiofmts = mConnection.getSupportedAudioFormats();
                for (final NvwfdAudioFormat audioformat : audiofmts) {
                    audioFormats.add(audioformat.toString());
                }
            }
            return audioFormats;
        }

        public List<String> getSupportedVideoFormats() {
            List<String> videoFormats = null;
            if (mConnection != null) {
                videoFormats = new ArrayList<String>();
                List<NvwfdVideoFormat> videofmts = mConnection.getSupportedVideoFormats();
                for (final NvwfdVideoFormat vidoeformat : videofmts) {
                    videoFormats.add(vidoeformat.toString());
                }
            }
            return videoFormats;
        }

        public String getActiveAudioFormat() {
            if (mConnection != null) {
                NvwfdAudioFormat audioFormat = mConnection.getActiveAudioFormat();
                return audioFormat.toString();
            }
            return null;
        }

        public String getActiveVideoFormat() {
            if (mConnection != null) {
                NvwfdVideoFormat videoFormat = mConnection.getActiveVideoFormat();
                return videoFormat.toString();
            }
            return null;
        }

        public void configurePolicyManager() {
            if (mNvwfdPolicyManager != null) {
                mNvwfdPolicyManager.configure(mContext, mNvwfd, mConnection);
                mNvwfdPolicyManager.start(new NvwfdAppListener(), mGameModeOption);
            }
        }

        public boolean updateConnectionParameters(int resoultionId) {
            if (true == mNvwfdPolicyManager.renegotiateResolution(resoultionId)) {
                mResolutionCheckId = resoultionId;
                return true;
            }
            return false;
        }

        public void forceResolution(boolean bValue) {
            mNvwfdPolicyManager.forceResolution(bValue);
            if (bValue == true) {
                mNvwfdPolicyManager.renegotiateResolution(mResolutionCheckId);
            }
            bForceResolution = bValue;
        }

        public void updatePolicy(int value) {
            mNvwfdPolicyManager.updatePolicy(value);
            mPolicySeekBarValue = value;
        }

        public int getResolutionCheckId() {
            return mResolutionCheckId;
        }

        public boolean getForceResolutionValue() {
            return bForceResolution;
        }

        public int getPolicySeekBarValue() {
            return mPolicySeekBarValue;
        }

        public boolean getSinkAvailabilityStatus(String sinkSSID) {
            String sinkId;
            synchronized(mSinksLock) {
                for (final NvwfdSinkInfo sink : mSinks) {
                    sinkId = sink.getSsid();
                    if (sinkId != null && sinkId.equalsIgnoreCase(sinkSSID)) {
                        return true;
                    }
                }
            }
            return false;
        }

        public List<String> getRememberedSinkList() {
            ArrayList<String> rememberedSinks = new ArrayList<String>(mSinkNickName.values());
            // arrange the remembered sinks list alphabetically
            Collections.sort(rememberedSinks);
            return rememberedSinks;
        }

        public boolean modifySink(String sinkSSID, boolean modify, String nickName) {
            synchronized(mHistoryDataLock) {
                String oldNickName = mSinkNickName.get(sinkSSID);
                if (oldNickName == null) {
                    Log.e(TAG, "not a remembered sink, sinkSSID" + sinkSSID);
                    return false;
                }
                if (modify) {
                    if (DBG) Log.d(TAG, "old nickname: " + oldNickName);
                    if (nickName != null && nickName.length() > 0) {
                        mSinkNickName.put(sinkSSID, nickName);
                        if (DBG) Log.d(TAG, "new nickName:" + nickName);
                    } else {
                        mSinkNickName.put(sinkSSID, sinkSSID);
                        if (DBG) Log.d(TAG, "new nickName:" + sinkSSID);
                    }
                    updateRememberedSinks();
                } else {
                    WifiP2pGroup selectedGroup = null;
                    String groupNetworkName = null;
                    groupNetworkName = mGroupId.get(sinkSSID);
                    if (DBG) Log.d(TAG, "remove from remembered sink list, sinkSSID"
                        + sinkSSID + " Group name = " + groupNetworkName);
                    if (DBG) Log.d(TAG, "mWifiP2pGroups size" + mWifiP2pGroups.size());
                    if (groupNetworkName != null && mWifiP2pManager != null && mChannel != null) {
                        for (WifiP2pGroup group: mWifiP2pGroups) {
                            if (group.getNetworkName().equalsIgnoreCase(groupNetworkName)) {
                                selectedGroup = group;
                                if (DBG) Log.d(TAG, " modifySink matched group " + group);
                                mWifiP2pManager.deletePersistentGroup(mChannel,
                                        selectedGroup.getNetworkId(),
                                        new WifiP2pManager.ActionListener() {
                                    public void onSuccess() {
                                        if (DBG) Log.d(TAG, " delete group success");
                                    }
                                    public void onFailure(int reason) {
                                        if (DBG) Log.d(TAG, " delete group fail " + reason);
                                    }
                                });
                                break;
                            }
                        }
                    }
                    try {
                        mBinder.removeNetworkGroupSink(groupNetworkName);
                    } catch(Exception e) {}
                }
            }
            // if connection is ongoing or already established, update the notification bar with new nickname
            if (getConnectedSinkId() != null) {
                if (isConnectionOngoing()) {
                    displayMiracastStatusNotification(NOTIF_CONNECTION_PROGRESS, getConnectedSinkId(), false, false);
                } else {
                    displayMiracastStatusNotification(NOTIF_STREAMING_ON, getConnectedSinkId(), false, false);
                }
            }
            return true;
        }

        public String getSinkNickname(String sinkSSID) {
            Set s = mSinkNickName.entrySet();
            if (DBG) Log.d(TAG, "in getSinkNickname, remembered set size:" + s.size());
            Iterator it = s.iterator();
            String sinkId = null;
            while (it.hasNext()) {
                Map.Entry m = (Map.Entry)it.next();
                if (DBG) Log.d(TAG, "nickName:" + m.getValue());
                sinkId = (String)m.getValue();
                if (sinkId != null && sinkId.equalsIgnoreCase(sinkSSID)) {
                    sinkSSID = (String)m.getKey();
                }
            }

            return mSinkNickName.get(sinkSSID);
        }

        public int getSinkFrequency(String sinkSSID) {
            return mSinkFrequency.get(sinkSSID);
        }

        public String getSinkSSID(String sinkNickName) {
            Set keySet = mSinkNickName.keySet();
            if (DBG) Log.d(TAG, "in getSinkSSID, remembered set size:" + keySet.size());
            Iterator it = keySet.iterator();
            String sinkId = null;
            String nickName = null;
            while (it.hasNext()) {
                sinkId = (String)it.next();
                if (DBG) Log.d(TAG, "sinkSSID:" + sinkId);
                nickName = mSinkNickName.get(sinkId);
                if (nickName != null && nickName.equalsIgnoreCase(sinkNickName)) {
                    break;
                }
                sinkId = null;
            }

            return sinkId;
        }

        public void cancelConnect() {
            if (DBG) Log.d(TAG, "Cancelling Connection....");
            Message msg = new Message();
            msg.what = CANCEL_CONNECTION_MSG;
            mServiceHandler.sendMessage(msg);
        }

        public String getSinkGroupNetwork(String sinkSSID) {
            if (DBG) Log.d(TAG, "getSinkGroupNetwork for sink: " + sinkSSID);
            return mGroupId.get(sinkSSID);
        }

        public List<String> getSinkNetworkGroupList() {
            return (new ArrayList<String>(mGroupId.values()));
        }

        public boolean removeNetworkGroupSink(String networkGroupName) {
            synchronized(mHistoryDataLock) {
                Set s = mGroupId.entrySet();
                if (DBG) Log.d(TAG, "in removeNetworkGroupSink, networkGroup set size:" + s.size());
                Iterator it = s.iterator();
                String networkName = null;
                while (it.hasNext()) {
                    Map.Entry m = (Map.Entry)it.next();
                    if (DBG) Log.d(TAG, "Network Group Name:" + m.getValue());
                    networkName = (String)m.getValue();
                    if (networkName != null && networkName.equalsIgnoreCase(networkGroupName)) {
                        String sinkSSID = (String)m.getKey();
                        mSinkNickName.remove(sinkSSID);
                        mGroupId.remove(sinkSSID);
                    }
                }
            }
            updateRememberedSinks();
            return true;
        }

        public boolean setGameModeOption(boolean enable) {
            if (DBG) Log.d(TAG, " in setGameModeOption enable: " + enable);
            synchronized(this) {
                SharedPreferences gameMode = mContext.getSharedPreferences(PREFS_NVWFD_UI_GAME_MODE, 0);
                SharedPreferences.Editor editor = gameMode.edit();
                mGameModeOption = enable;
                editor.putBoolean("game_mode", mGameModeOption);
                editor.commit();
                if (mConnection != null) {
                    mNvwfdPolicyManager.setGamingMode(enable);
                }
            }
            return true;
        }

        public boolean getGameModeOption() {
            return mGameModeOption;
        }

        public void pause() {
            if (mConnectedSink == null) return;
            if (DBG) Log.d(TAG, "pause+ : " + mConnectedSink.getSsid());

            Message msg = new Message();
            msg.what = PAUSE_MSG;
            msg.obj = null;
            mServiceHandler.sendMessage(msg);
        }

        public void resume() {
            if (mConnectedSink == null) return;
            if (DBG) Log.d(TAG, "resume+ : " + mConnectedSink.getSsid());

            Message msg = new Message();
            msg.what = RESUME_MSG;
            msg.obj = null;
            mServiceHandler.sendMessage(msg);
        }
    };

    private void fileCleanup(ObjectOutputStream oos, FileOutputStream fout) {
        try {
            oos.flush();
            oos.close();
            fout.flush();
            fout.close();
        } catch (Exception ex) {
            Log.e(TAG, "Exception in fileCleanup:");
        }
    }

    private void readRememberedInfoFromFile() {
        try {
            synchronized(this) {
                File file = new File("/data/data/com.nvidia.NvWFDSvc/remembered_sinks.conf");
                if (file.exists()) {
                    FileInputStream fin = new FileInputStream(file);
                    ObjectInputStream ois = new ObjectInputStream(fin);
                    mSinkNickName = (HashMap<String, String>)ois.readObject();
                    ois.close();
                    fin.close();
                } else {
                    Log.e(TAG, "file doesn't exist:" + file.getPath());
                }
                file = new File("/data/data/com.nvidia.NvWFDSvc/sinks_frequency.conf");
                if (file.exists()) {
                    FileInputStream fin = new FileInputStream(file);
                    ObjectInputStream ois = new ObjectInputStream(fin);
                    mSinkFrequency = (HashMap<String, Integer>)ois.readObject();
                    ois.close();
                    fin.close();
                } else {
                    Log.e(TAG, "file doesn't exist:" + file.getPath());
                }
                file = new File("/data/data/com.nvidia.NvWFDSvc/remembered_sink_group.conf");
                if (file.exists()) {
                    FileInputStream fin = new FileInputStream(file);
                    ObjectInputStream ois = new ObjectInputStream(fin);
                    mGroupId = (HashMap<String, String>)ois.readObject();
                    ois.close();
                    fin.close();
                } else {
                    Log.e(TAG, "file doesn't exist:" + file.getPath());
                }
                if (Integer.parseInt(SystemProperties.get("nvwfd.gamemode", "0")) == 1) {
                    SharedPreferences gameMode = mContext.getSharedPreferences(PREFS_NVWFD_UI_GAME_MODE, 0);
                    mGameModeOption = gameMode.getBoolean("game_mode", DEFAULT_GAME_MODE_VALUE);
                }
            }
        } catch (Exception ex) {
            Log.e(TAG, "Exception in readRememberedInfoFromFile:");
        }
    }

    private void updateRememberedSinks() {
        try {
            synchronized(this) {
                File file = new File("/data/data/com.nvidia.NvWFDSvc/remembered_sinks.conf");
                FileOutputStream fout = new FileOutputStream(file);
                ObjectOutputStream oos = new ObjectOutputStream(fout);
                oos.writeObject(mSinkNickName);
                // flush doesn't guarantee that object will be written onto the physical disk immediately
                fileCleanup(oos, fout);

                file = new File("/data/data/com.nvidia.NvWFDSvc/sinks_frequency.conf");
                fout = new FileOutputStream(file);
                oos = new ObjectOutputStream(fout);
                oos.writeObject(mSinkFrequency);
                fileCleanup(oos, fout);

                file = new File("/data/data/com.nvidia.NvWFDSvc/remembered_sink_group.conf");
                fout = new FileOutputStream(file);
                oos = new ObjectOutputStream(fout);
                oos.writeObject(mGroupId);
                fileCleanup(oos, fout);
            }
        } catch (Exception ex) {
            Log.e(TAG, "Exception in updateRememberedSinks:");
        }
    }

     private class NvwfdListener implements Nvwfd.NvwfdListener {
        @Override
        public void onDiscoverSinks(List<NvwfdSinkInfo> sinkList) {
            synchronized(mSinksLock) {
                mSinks.clear();
                for (final NvwfdSinkInfo nvwfdSink : sinkList) {
                    String sinkSsid = nvwfdSink.getSsid();
                    Log.d(TAG, "Sink " + sinkSsid +" found After :"+ (System.currentTimeMillis() -mDiscoveryTs) + " millisecs");
                    mSinks.add(nvwfdSink);
                }
            }
            mServiceHandler.sendEmptyMessage(SINKS_FOUND_MSG);
        }
        @Override
        public void onConnect(NvwfdConnection connection, int errorCode) {
            if (connection == null) {
                Log.e(TAG, "onConnect:: Connection is NULL failed errorCode: " + errorCode);
            } else {
                if (DBG) Log.d(TAG, "onConnect:: Connection succeeded");
            }
            mConnection = connection;
            Message msg = new Message();
            msg.arg1 = errorCode;
            msg.what = CONNECT_DONE_MSG;
            mServiceHandler.sendMessage(msg);
        }
        @Override
        public void discoverSinksResult(boolean result) {
            if (DBG) Log.d(TAG, "discoverSinksResult " + result);
            Message msg = new Message();
            msg.arg1 = (result == true) ? 1 : 0;
            msg.what = DISCOVERY_STATUS_MSG;
            mServiceHandler.sendMessage(msg);
        }
    }

    private void onSetupComplete() {
        if (DBG) Log.d(TAG, "onSetupComplete + ");
        synchronized(mClientsLock) {
            for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                if (mNvwfdServiceListenerCallback != null) {
                    try {
                        mNvwfdServiceListenerCallback.onSetupComplete(0);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        }
        if (DBG) Log.d(TAG, "onSetupComplete -");
    }

    private class NvwfdServiceStateListener implements Nvwfd.NvwfdServiceStateListener {
        @Override
        public void onServiceConnected() {
            if (DBG)  Log.d(TAG, "********In onServiceConnected CallBack***** ");
            onSetupComplete();
        }
        @Override
        public void onServiceDied() {
            Log.e(TAG, "***************In OnServiceDied CallBack************  ");
            setForeGround(false, null, -1);
            mServiceHandler.removeMessages(SINKS_FOUND_MSG);
            mServiceHandler.removeMessages(CONNECT_DONE_MSG);
            mServiceHandler.removeMessages(DISCONNECT_START_MSG);
            mServiceHandler.removeMessages(DISCOVERY_STATUS_MSG);
            mServiceHandler.removeMessages(DISCONNECT_DONE_MSG);
            mServiceHandler.removeMessages(PENDING_CONNECT_MSG);
            mServiceHandler.removeMessages(PAUSE_MSG);
            mServiceHandler.removeMessages(RESUME_MSG);
            mServiceHandler.sendEmptyMessage(SERVICE_DIED);
        }
        @Override
        public void onServiceRestarted() {
            if (DBG) Log.d(TAG, "**************In OnServiceRestarted CallBack*********  ");
            mServiceHandler.sendEmptyMessage(SERVICE_RESTARTED);
        }
    }

    private class NvwfdCancelConnectListener implements Nvwfd.NvwfdCancelConnectListener {

        @Override
        public void onSuccess() {
            if (DBG) Log.d(TAG, "Cancel Connection successful");
            synchronized(mClientsLock) {
                cancelNotification(0);
                reset();
                for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                    if (mNvwfdServiceListenerCallback != null) {
                        try {
                            mNvwfdServiceListenerCallback.onCancelConnect(1);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                }
             }
        }

        @Override
        public void onFailure(int reason) {
            String text = null;
            switch (reason) {
                case WifiP2pManager.P2P_UNSUPPORTED:
                    text = "P2P_UNSUPPORTED";
                    break;
                case WifiP2pManager.ERROR:
                    text = "ERROR";
                    break;
                case WifiP2pManager.BUSY:
                    text = "BUSY";
                    break;
            }
            if (DBG) Log.d(TAG, "Connection Cancellation Failed,Reason: "+ text);
            synchronized(mClientsLock) {
                reset();
                cancelNotification(0);
                for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                    if (mNvwfdServiceListenerCallback != null) {
                        try {
                            mNvwfdServiceListenerCallback.onCancelConnect(0);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                }
             }
        }
    }

    private class NvwfdAppListener implements NvwfdPolicyManager.NvwfdAppListener {
        @Override
        public void onDisconnect() {
            if (DBG) Log.d(TAG, "onDisconnect - detection from policy manager  ");
            mDisconnectionTs = System.currentTimeMillis();
            Message msg = new Message();
            msg.what = DISCONNECT_START_MSG;
            msg.obj = null;
            mServiceHandler.sendMessage(msg);
        }

        @Override
        public void onHdcpstatus(int status) {
            if ((status == NvwfdHdcpInfo.HDCP_AUTH_FAILURE) || (status == NvwfdHdcpInfo.HDCP_AUTH_COMPROMISED)) {
                if (DBG) Log.d(TAG, "HdcpAuthenticationFailure - detection from policy manager  ");
                Message msg = new Message();
                mServiceHandler.sendEmptyMessage(HDCP_AUTH_FAILED);
            }
        }

        @Override
        public void onStreamStatus(int status) {
            if (DBG) Log.d(TAG, "onStreamStatus - detection from policy manager  :: status = " + status);
        }
    }

    private class NvwfdDisconnectListener implements NvwfdConnection.NvwfdDisconnectListener {

        @Override
        public void onDisconnectResult(boolean result) {
            if (DBG) Log.d(TAG, "onDisconnectResult " + result);
            Message msg = new Message();
            msg.what = DISCONNECT_DONE_MSG;
            mServiceHandler.sendMessage(msg);
        }
    };

    // Handler that receives messages from the thread
    private final class ServiceHandler extends Handler {
        public ServiceHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (DBG) Log.d(TAG, "Message: " + msg);
            handleNotification(msg);
        }
    }

    private void addSinkAsRemembered(WifiP2pGroup p2pGroup) {
        synchronized(mHistoryDataLock) {
            String sinkSsid = mConnectedSink.getSsid();
            if (!mSinkNickName.containsKey(sinkSsid)) {
                mSinkNickName.put(sinkSsid, sinkSsid);
            }
            if (!mGroupId.containsKey(sinkSsid)) {
                mGroupId.put(sinkSsid, p2pGroup.getNetworkName());
                if (DBG) Log.d(TAG, " mGroupId size: " + mGroupId.size());
                mSinkFrequency.put(sinkSsid, 1);
            }
            updateRememberedSinks();
        }
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {
                if (mWifiP2pManager != null && mChannel != null) {
                    mWifiP2pManager.requestPersistentGroupInfo(mChannel, NvwfdService.this);
                }
                NetworkInfo info = (NetworkInfo)intent.getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);
                if (DBG) Log.d(TAG, "onReceive info.isConnected() = " + info.isConnected());
                 if (info.isConnected()) {
                    WifiP2pGroup p2pGroup = (WifiP2pGroup)intent.getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_GROUP);
                    if (p2pGroup != null) {
                        WifiP2pDevice groupOwner = p2pGroup.getOwner();
                        if (mConnectedSink!= null && groupOwner != null &&
                            mConnectedSink.getMacAddress().equalsIgnoreCase(groupOwner.deviceAddress)) {
                            if (DBG) Log.d(TAG, "connection intiated sink is a group owner:" + groupOwner.deviceAddress);
                            addSinkAsRemembered(p2pGroup);
                        } else {
                            ArrayList<WifiP2pDevice> devices = new ArrayList(p2pGroup.getClientList());
                            if (DBG) Log.d(TAG, "group client size " + devices.size());
                            for (WifiP2pDevice peer: devices) {
                                if (mConnectedSink!= null &&
                                    mConnectedSink.getMacAddress().equalsIgnoreCase(peer.deviceAddress)) {
                                    if (DBG) Log.d(TAG, "connection intiated sink is a client in group = " + peer.deviceAddress);
                                    addSinkAsRemembered(p2pGroup);
                                }
                            }
                        }
                    }
                }
            } else if (MIRACAST_DISCONNECT_ACTION.equals(action)) {
                if (DBG) Log.d(TAG, "User requested disconnect, Disconnecting the Miracast session");
                try {
                    if (mBinder.isConnectionOngoing()) {
                        if (DBG) Log.d(TAG, "Cancelling ongoing Miracast connection");
                        mBinder.cancelConnect();
                    } else if (mBinder.isConnected()) {
                        if (DBG) Log.d(TAG, "Disconnecting Miracast connection");
                        mBinder.disconnect();
                    } else {
                        if (DBG) Log.d(TAG, "No active Miracast connection, User Switch has no action");
                    }
                } catch (Exception e) {}
            } else if (MIRACAST_FREQ_CONFLICT_ACTION.equals(action)) {
                mIsUserDisconnected = true;
            } else if (WifiP2pManager.WIFI_P2P_PERSISTENT_GROUPS_CHANGED_ACTION.equals(action)) {
                if (DBG) Log.d(TAG, "Persistent group changed: ");
                if (mWifiP2pManager != null && mChannel != null) {
                    mWifiP2pManager.requestPersistentGroupInfo(mChannel, NvwfdService.this);
                }
            } else if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(action)) {
                mWifiP2PState = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1);
                if (DBG) Log.d(TAG, "got WIFI_P2P_STATE_CHANGED_ACTION broadcast " + mWifiP2PState);
            }
        }
    };

    public void onPersistentGroupInfoAvailable(WifiP2pGroupList groups) {
        WifiManager wifimgr = (WifiManager)getSystemService(Context.WIFI_SERVICE);
        boolean wifiIsOn = wifimgr.isWifiEnabled();

        synchronized(mHistoryDataLock) {
            if (wifiIsOn && (mWifiP2PState == WifiP2pManager.WIFI_P2P_STATE_ENABLED)) {
                List<String> groupNetworkNameList = new ArrayList<String>();
                mWifiP2pGroups.clear();
                if (DBG) Log.d(TAG, " onPersistentGroupInfoAvailable groups size " + groups.getGroupList().size());
                for (WifiP2pGroup group: groups.getGroupList()) {
                    mWifiP2pGroups.add(group);
                    groupNetworkNameList.add(group.getNetworkName());
                }

                // Remove the group id from NvwfdService database
                // if the group is removed using other apps eg: Wi-Fi Direct
                try {
                    List<String> sinkNetworkGroupList = mBinder.getSinkNetworkGroupList();
                    if (sinkNetworkGroupList != null) {
                        if (DBG) Log.d(TAG, " sinkNetworkGroupList size: " + sinkNetworkGroupList.size()
                            + "group Network Name size: " + groupNetworkNameList.size());
                        for (String sinkNetworkGroupName : sinkNetworkGroupList) {
                            if (DBG) Log.d(TAG, " sinkNetworkGroupName: " + sinkNetworkGroupName);
                            if (!groupNetworkNameList.contains(sinkNetworkGroupName)) {
                                mBinder.removeNetworkGroupSink(sinkNetworkGroupName);
                            }
                        }
                    }
                } catch(Exception e) {}
            }
        }
    }

    @Override
    public void onCreate() {
        // Start the thread running the service and in background priority.
        if (DBG) Log.d(TAG, "StartAtBootService Created");
        mNotificationManager = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        HandlerThread thread = new HandlerThread("ServiceStartArguments",
            Process.THREAD_PRIORITY_BACKGROUND);

        thread.start();
        mContext = getApplicationContext();
        // Get the HandlerThread's Looper and use it for our Handler
        mServiceLooper = thread.getLooper();
        mServiceHandler = new ServiceHandler(mServiceLooper);

        mMiracastSettingsIntent = new Intent("com.nvidia.settings.MIRACAST_SETTINGS");
        mMiracastSettingsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mNvwfd = new Nvwfd();
        mNvwfdPolicyManager = new NvwfdPolicyManager();
        mNvwfdListener = new NvwfdListener();
        mServiceStateListener = new NvwfdServiceStateListener();
        mSinks = new ArrayList<NvwfdSinkInfo>();
        mBusySinksList = new ArrayList<String>();
        mResolutionCheckId = NvwfdPolicyManager.CONNECTION_720P_30FPS;
        bForceResolution = false;
        mPolicySeekBarValue = 50;

        mSinkNickName = new HashMap<String, String>();
        mClients = new HashMap<Integer, INvWFDServiceListener>();
        mClientsLock = new Object();
        mSinkFrequency = new HashMap<String, Integer>();

        mIntentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
        mIntentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
        mIntentFilter.addAction(WifiP2pManager.WIFI_P2P_PERSISTENT_GROUPS_CHANGED_ACTION);
        // add intent when user is requsted for Miracast session disconnect to start PGC
        mIntentFilter.addAction(MIRACAST_DISCONNECT_ACTION);
        //add intent when frequency conflict occurs and miracast is disconnected
        mIntentFilter.addAction(MIRACAST_FREQ_CONFLICT_ACTION);
        mGroupId = new HashMap<String, String>();
        mSinksLock = new Object();
        mHistoryDataLock = new Object();
        addService();
        mWifiP2pGroups = new ArrayList<WifiP2pGroup>();
        if (Integer.parseInt(SystemProperties.get("nvwfd.gamemode", "0")) == 1) {
            SharedPreferences gameMode = mContext.getSharedPreferences(PREFS_NVWFD_UI_GAME_MODE, 0);
            mGameModeOption = gameMode.getBoolean("game_mode", DEFAULT_GAME_MODE_VALUE);
        }
    }

    private void addService() {
        if (DBG) Log.d(TAG, "addService :: Registering NvWFDSvc with Android");
        try {
            Class.forName("android.os.ServiceManager").getMethod("addService", new Class[] {
               String.class, IBinder.class}).invoke(null, new Object[] {"com.nvidia.NvWFDSvc.NvwfdService", mBinder});
            if (DBG) Log.d(TAG,"addService on ServiceManager successful");
        } catch (Exception ex) {
            // if we get this when the current user is not the owner, it's expected.
            // be more silent
            Log.e(TAG,"****Could not add NvWFDService to Android ServiceManager****");
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // start request be handled by message. start ID to track request on finish the work etc.
        if (DBG) Log.d(TAG, "onStartCommand:: Starting #" + startId);
        setForeGround(false, null, -1);
        if(intent == null) { //null intent indicates service has been restarted
            cancelNotification(0);
            if (mNvwfdPolicyManager != null) {
                if (DBG) Log.d(TAG, "onStartCommand:: stopping rbe after service restart");
                mNvwfdPolicyManager.stop();
            }
        }
        // If we get killed, after returning from here, restart
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DBG) Log.d(TAG, "onBind ,," + intent.toString());
        return mBinder;
    }

    @Override
    public void onRebind (Intent intent) {
        if (DBG) Log.d(TAG, "onRebind: got intent " + intent);
    }

    @Override
    public boolean onUnbind (Intent intent) {
        if (DBG) Log.d(TAG, "onUnbind: got intent " + intent);
        if (mConnection == null) {
            setForeGround(false, null, -1);
        }
        return true;
    }

    @Override
    public void onTrimMemory (int level) {
        Log.e(TAG, "onTrimMemory lvl = " + level);
    }

    @Override
    public void onDestroy() {
        if (DBG) Log.d(TAG, "onDestroy");
    }

    void connectionTime() {
        Log.d(TAG, "QoS:Time for connection is: "+
            (System.currentTimeMillis() -mStartConnectionTs) + " millisecs");
    }

    void disConnectionTime() {
        Log.d(TAG, "QoS:Time for disconnection is: "+ (System.currentTimeMillis()-mDisconnectionTs) + " millisecs");
    }

    void reset() {
        if (mNvwfdPolicyManager != null) {
            mNvwfdPolicyManager.stop();
            Log.d(TAG, "QoS:No of Frames Dropped : "+ (NvwfdPolicyManager.mFramedropCount));
            mResolutionCheckId = NvwfdPolicyManager.CONNECTION_720P_30FPS;
            bForceResolution = false;
            mPolicySeekBarValue = 50;
        }
        mConnection = null;
        mConnectedSink = null;
        connectedSinkId = null;
        mPendingConnectedSink = null;
    }

    private void handleNotification(Message msg) {
        switch (msg.what) {
            case SINKS_FOUND_MSG:
            {
                if (isDiscoveryInitiated && mPendingConnectedSink != null) {
                    synchronized(mSinksLock) {
                        for (final NvwfdSinkInfo tempSink : mSinks) {
                            if ((tempSink != null) && mPendingConnectedSink.equals(tempSink.getSsid())) {
                                if (DBG) Log.d(TAG, "Found sink device for which connection was pending " + tempSink.getSsid());
                                mServiceHandler.removeMessages(PENDING_CONNECT_MSG);
                                mConnectedSink = tempSink;
                                mNvwfd.connect(tempSink, (mPendingAuth==null) ? false:true,
                                          mNvwfdPolicyManager.getDefaultVideoFormat(mGameModeOption), null);
                                try {
                                    displayMiracastStatusNotification(NOTIF_CONNECTION_PROGRESS, mBinder.getConnectedSinkId(), false, true);
                                } catch(Exception e) {}
                                mPendingConnectedSink = null;
                                mPendingAuth = null;
                                isDiscoveryInitiated = false;
                                break;
                            }
                        }
                    }
                }

                List<String> displaySinkList = null;
                try {
                    // get the available sinks and update the busy sinks
                    List<String> sinkList = mBinder.getSinkList();
                    Set displaySet = new LinkedHashSet<String>();

                    // create the remembered sinks list
                    List<String>rememberedSinks = new ArrayList<String>();
                    Set rememberedSinksSet = mSinkNickName.entrySet();
                    Iterator it = rememberedSinksSet.iterator();
                    String sinkId = null;
                    while (it.hasNext()) {
                        Map.Entry m = (Map.Entry)it.next();
                        if (DBG) Log.d(TAG, "sink Name: " + (String)m.getKey());
                        rememberedSinks.add((String)m.getKey());
                    }

                    Set history = new HashSet<String>(rememberedSinks);
                    if (DBG) Log.d(TAG, "size of history:" + history.size());

                    Set busySinks = new HashSet<String>(mBusySinksList);
                    if (DBG) Log.d(TAG, "size of busy sink list:" + busySinks.size());

                    // create list of sinks which are available, remembered and not busy
                    Set oldAvailableSinks = new HashSet<String>(sinkList);
                    oldAvailableSinks.retainAll(history);
                    oldAvailableSinks.removeAll(mBusySinksList);

                    // create list of sinks which are available, not remembered and not busy
                    Set newAvailableSinks = new HashSet<String>(sinkList);
                    newAvailableSinks.removeAll(oldAvailableSinks);
                    newAvailableSinks.removeAll(mBusySinksList);
                    if (history.size() > 0) {
                        // sinks list with (old available + new available + busy sinks)
                        displaySet.addAll(oldAvailableSinks);
                        displaySet.addAll(newAvailableSinks);
                        displaySet.addAll(busySinks);
                    } else {
                        Set availableSinks = new HashSet<String>(sinkList);
                        // create list of sinks which are available and not busy
                        availableSinks.removeAll(mBusySinksList);
                        // sinks list with (available + available and busy).
                        displaySet.addAll(availableSinks);
                        displaySet.addAll(busySinks);
                    }
                    displaySinkList = new ArrayList<String>(displaySet);

                    if (displaySinkList != null) {
                        Collections.reverse(displaySinkList);
                        if (history.size() > 0) {
                            // create list of sinks which are remembered and not available in alphabetical order
                            history.removeAll(sinkList);
                            List<String> historySinkList = new ArrayList<String>(history);
                            // Create a remembered sink nickname hash map with sink nickname as key and value as sink name.
                            HashMap<String, String> rememberedSinkName = new HashMap<String, String>();
                            List<String>rememberedSinkNicknames = new ArrayList<String>();
                            // create the remembered sinks nickname list
                            for (final String rememberedSink : historySinkList) {
                                String sinkNickname = (String)mSinkNickName.get(rememberedSink);
                                if (sinkNickname != null) {
                                    rememberedSinkNicknames.add(sinkNickname);
                                    rememberedSinkName.put(sinkNickname, rememberedSink);
                                }
                            }
                            // sort the sinks nickname list
                            Collections.sort(rememberedSinkNicknames);
                            // create the remembered sinks list based on sorted sinks nickname list
                            historySinkList.clear();
                            for (final String rememberedSinkNickName : rememberedSinkNicknames) {
                                historySinkList.add((String)rememberedSinkName.get(rememberedSinkNickName));
                            }

                            // add (remembered and not available) sinks list to remaining sinks list
                            for (final String sink : historySinkList) {
                                displaySinkList.add(0, sink);
                            }
                        }
                        if (mBinder.isConnected()) {
                            /* Add the connected or conneciton establishment ongoing sink name at the end of the sinks list,
                                so that it will be displayed at the top of sinks discovered UI*/
                            if (mConnectedSink != null && mConnectedSink.getSsid() != null) {
                                if (DBG) Log.d(TAG, "connected sink name: " + mConnectedSink.getSsid());
                                if(mBinder.isConnectionOngoing()) {
                                    displaySinkList.remove(mConnectedSink.getSsid());
                                } else {
                                    displaySinkList.clear();
                                }
                                displaySinkList.add(displaySinkList.size(), mConnectedSink.getSsid());
                            }
                        }

                        synchronized(mClientsLock) {
                            for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                                if (mNvwfdServiceListenerCallback != null) {
                                    try {
                                        mNvwfdServiceListenerCallback.onSinksFound(displaySinkList);
                                    } catch (Exception e) {
                                        e.printStackTrace();
                                        Log.e(TAG, "Exception");
                                    }
                                }
                            }
                        }
                    }
                } catch (Exception ex) {
                    ex.printStackTrace();
                    Log.e(TAG, "Exception" + ex.getMessage());
                }
                if (DBG) Log.d(TAG, "handleNotification::Miracast SINKS_FOUND_MSG");
                break;
            }
            case CONNECT_DONE_MSG:
            {
                if (mConnectedSink != null) {
                    boolean connSuccess = (mConnection != null) ? true : false;
                    if (DBG) Log.d(TAG, "handleNotification::Miracast CONNECT_DONE_MSG  connSuccess = " + connSuccess + "  sink = " + mConnectedSink);

                    if (connSuccess) {
                        connectionTime();
                        connectedSinkId = mConnectedSink.getSsid();
                        doRequiredInSettingsLocation(NvwfdService.this, connSuccess);
                        String nickName = null;
                        synchronized(mHistoryDataLock) {
                            nickName = mSinkNickName.get(connectedSinkId);
                            if (nickName == null) {
                                if (DBG) Log.d(TAG, "connect sink name:" + connectedSinkId);
                                nickName = connectedSinkId;
                                mSinkNickName.put(connectedSinkId, connectedSinkId);
                                if (DBG) Log.d(TAG, "On connect done remembered set size:" + mSinkNickName.size());
                                mSinkFrequency.put(connectedSinkId, 1);
                            } else {
                                int frequencyCount = mSinkFrequency.get(connectedSinkId);
                                frequencyCount++;
                                mSinkFrequency.put(connectedSinkId, frequencyCount);
                                if (DBG) Log.d(TAG, "new frequency:" + frequencyCount);
                            }
                        }
                        updateRememberedSinks();

                         // Configure Policy Manager
                        try {
                            mBinder.configurePolicyManager();
                        } catch (Exception ex) {}
                        if (mConnection != null) {
                            if (DBG) Log.d(TAG, "registerDisconnectionListener from service");
                            mConnection.registerDisconnectListener(new NvwfdDisconnectListener());
                        }

                         //Using nickName to display
                         displayMiracastStatusNotification(NOTIF_STREAMING_ON, nickName, false, true);

                         if (bHDMIActiveAtConnect) {
                             displayHDMINotification(R.string.miracast_hdmi_title, R.string.miracast_hdmi_unavailable);
                         }

                         msg = new Message();
                         msg.what = SINKS_FOUND_MSG;
                         mServiceHandler.sendMessage(msg);
                    } else {
                        Log.e(TAG,"Connection failure");
                        if (msg.arg1 == -2) {
                            cancelNotification(0);
                            reset();
                            synchronized(mClientsLock) {
                                for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                                    if (mNvwfdServiceListenerCallback != null) {
                                        try {
                                            mNvwfdServiceListenerCallback.onCancelConnect(0);
                                        } catch (Exception e) {
                                            e.printStackTrace();
                                        }
                                    }
                                }
                             }
                            break;
                        } else {
                            try {
                                if (mBinder.getConnectedSinkId() != null && mBinder.isConnectionOngoing()) {
                                    displayMiracastStatusNotification(NOTIF_WARNING_AUTHENTIFICATION_FAILED, mBinder.getConnectedSinkId(), true, true);
                                }
                            } catch (Exception ex) {}
                        }
                        reset();
                    }
                    synchronized(mClientsLock) {
                        for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                            if (mNvwfdServiceListenerCallback != null) {
                                try {
                                    mNvwfdServiceListenerCallback.onConnectDone(connSuccess);
                                } catch (Exception e) {
                                    e.printStackTrace();
                                }
                            }
                        }
                    }
                } else {
                    // mConnectedSink should not be null as it is being used to determine (being connected)/connected condition
                    // while we are being connected to one of the sink, we take no action on connect from same or different client
                    Log.e(TAG, " handleNotification::Miracast CONNECT_DONE_MSG  mConnectedSink is null !!!!!!");
                }
                break;
            }
            case DISCONNECT_START_MSG:
            {
                if (DBG) Log.d(TAG, "handleNotification::Miracast DISCONNECT_START_MSG");
                try {
                    mDisconnectedSink = mBinder.getConnectedSinkId();
                } catch (Exception ex) {}
                NvwfdConnection connection = mConnection;
                reset();
                if (connection != null) {
                    connection.disconnect();
                }
                break;
            }
            case DISCONNECT_DONE_MSG:
            {
                disConnectionTime();
                doRequiredInSettingsLocation(NvwfdService.this, false);
                setForeGround(false, null, -1);
                if (DBG) Log.d(TAG, "handleNotification::Miracast DISCONNECT_DONE_MSG");
                try {
                    if (mDisconnectedSink == null) {
                        mDisconnectedSink = mBinder.getConnectedSinkId();
                    }
                    if (mIsUserDisconnected) {
                        cancelNotification(0);
                    }
                    if (mDisconnectedSink != null && !mIsUserDisconnected) {
                        displayMiracastStatusNotification(NOTIF_WARNING_CONN_LOST, mDisconnectedSink, true, true);
                    }
                    mIsUserDisconnected = false;
                } catch (Exception ex) {}

                reset();
                synchronized(mClientsLock) {
                    for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                        if (mNvwfdServiceListenerCallback != null) {
                            try {
                                mNvwfdServiceListenerCallback.onDisconnectDone(true);
                            } catch (Exception e) {
                                e.printStackTrace();
                                Log.e(TAG, "Exception");
                            }
                        }
                    }
                    if (mClients.size() == 0) {
                        deInitialize();
                    }
                }
                break;
            }
            case DISCOVERY_STATUS_MSG:
            {
                if (DBG) Log.d(TAG, "handleNotification::Miracast DISCOVERY_STATUS_MSG");
                if (msg.arg1 == 1) {
                    if (DBG) Log.d(TAG, "handleNotification::Miracast Discovery Success");
                } else {
                    Log.e(TAG, "handleNotification::Miracast Discovery Fail");
                }
                synchronized(mClientsLock) {
                    for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                        if (mNvwfdServiceListenerCallback != null) {
                            try {
                                mNvwfdServiceListenerCallback.onDiscovery(msg.arg1);
                            } catch (Exception e) {
                                e.printStackTrace();
                                Log.e(TAG, "Exception");
                            }
                        }
                    }
                }
                break;
            }
            case PENDING_CONNECT_MSG:
            {
                if (DBG) Log.d(TAG, "handleNotification::Miracast PENDING_CONNECT_MSG");
                if (isDiscoveryInitiated && mPendingConnectedSink != null) {
                    /* mPendingConnectedSink has already updated with sink ssid using nickname, can be used directly
                        for connect method*/
                    synchronized(mSinksLock) {
                        for (final NvwfdSinkInfo sink : mSinks) {
                            String sinkSsid = sink.getSsid();
                            if (DBG) Log.d(TAG, "connectingSinkId " + sinkSsid);
                            if (sinkSsid != null && sinkSsid.equalsIgnoreCase(mPendingConnectedSink)) {
                                // Attempt connection
                                if (DBG) Log.d(TAG, "Attempting connect to " + sink.getSsid());
                                mStartConnectionTs = System.currentTimeMillis();
                                mConnectedSink = sink;
                                mNvwfd.connect(sink, (mPendingAuth==null) ? false:true,
                                                 mNvwfdPolicyManager.getDefaultVideoFormat(mGameModeOption), null);
                                try {
                                    displayMiracastStatusNotification(NOTIF_CONNECTION_PROGRESS, mBinder.getConnectedSinkId(), false, true);
                                } catch(Exception e) {}
                                mPendingConnectedSink = null;
                                mPendingAuth = null;
                                isDiscoveryInitiated = false;
                                return;
                            }
                        }
                    }

                    mConnectedSink = null;
                    try {
                        if (mBinder.getConnectedSinkId() != null && mBinder.isConnectionOngoing()) {
                            displayMiracastStatusNotification(NOTIF_WARNING_CONN_FAILED, mBinder.getConnectedSinkId(), true, true);
                        }
                    } catch (Exception ex) {}
                    mPendingConnectedSink = null;
                    mPendingAuth = null;
                    isDiscoveryInitiated = false;

                    if (DBG) Log.d(TAG, "Sink is not available. Discover sinks again.");
                     synchronized(mClientsLock) {
                          for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                              if (mNvwfdServiceListenerCallback != null) {
                                   try {
                                        mNvwfdServiceListenerCallback.onConnectDone(false);
                                   } catch (Exception e) {
                                        e.printStackTrace();
                                   }
                              }
                          }
                     }
                }
                break;
            }
            case SERVICE_DIED:
            {
                try {
                    mDisconnectedSink = mBinder.getConnectedSinkId();
                    reset();
                    if (mDisconnectedSink != null) {
                        displayMiracastStatusNotification(NOTIF_WARNING_CONN_LOST, mDisconnectedSink, true, true);
                    } else {
                        if (DBG) Log.d(TAG,"mDisconnectedSink is null - clearing the notification");
                        cancelNotification(0);
                    }
                } catch (Exception ex) {
                    ex.printStackTrace();
                }
                synchronized(mClientsLock) {
                    for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                        if (mNvwfdServiceListenerCallback != null) {
                            try {
                                mNvwfdServiceListenerCallback.onNotifyError("ServiceDied");
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    }
                }
                break;
            }
            case SERVICE_RESTARTED:
            {
                synchronized(mClientsLock) {
                    for (final INvWFDServiceListener mNvwfdServiceListenerCallback : mClients.values()) {
                        if (mNvwfdServiceListenerCallback != null) {
                            try {
                                mNvwfdServiceListenerCallback.onNotifyError("ServiceRestarted");
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    }
                    if (mClients.size() == 0) {
                        deInitialize();
                    }
                }
                break;
            }
            case CANCEL_CONNECTION_MSG:
            {
                mNvwfd.cancelConnect(new NvwfdCancelConnectListener());
                break;
            }
            case HDCP_AUTH_FAILED:
            {
                try {
                    if (mBinder.getConnectedSinkId() != null) {
                        displayHDCPNotification(R.drawable.ic_miracast_warning, mBinder.getConnectedSinkId());
                    }
                } catch (Exception ex) {}
                break;
            }
            case PAUSE_MSG:
            {
                if (mConnection != null) {
                    mConnection.pause();
                }
                break;
            }
            case RESUME_MSG:
            {
                if (mConnection != null)
                    mConnection.resume();
                break;
            }
            default:
            {
                Log.e(TAG, "handleNotification::handle default message");
                break;
            }
        }
    }

    public static String readHDMISwitchState() {
        FileReader hdmiStateFile = null;
        try {
            char[] buffer = new char[1024];
            hdmiStateFile = new FileReader("/sys/class/switch/tegradc.1/state");
            int len = hdmiStateFile.read(buffer, 0, 1024);

            String hdmiState = (new String(buffer, 0, len)).trim();

            return hdmiState;
        } catch (FileNotFoundException e) {
            return null;
        } catch (Exception e) {
            return null;
        } finally {
            if (hdmiStateFile != null) {
                try {
                    hdmiStateFile.close();
                } catch (Exception ignored) {}
            }
        }
    }

    public static boolean IsHDMIConnected() {
        String hdmiState = readHDMISwitchState();

        if (hdmiState == null) {
            // no HDMI
            return false;
        }

        // The expected return from HDMI switch state is either "offline" (not connected)
        // or "<width>x<height>" implying connected.
        String[] strDims = hdmiState.split("x");
        if (strDims.length == 2) {
            return true;
        }
        return false;
    }

    private void displayNotification(Intent intent, int id, String title, String title2, boolean autocancel, boolean showTickerText) {
        // WAR: sometimes when sink gets discovered quicky connection notif is shown before search notif animation could complete
        // this causing connection notif shown twice quickly. search and connection notifs share same id, so no need to cancel
        if(mPrevNotifId != id) {
            //cancel all notification first
            cancelNotification(0);
        }
        mPrevNotifId = id;

        Notification.Builder builder = new Notification.Builder(mContext);
        PendingIntent pendingIntent = PendingIntent.getActivityAsUser(mContext, 0, intent, 0, null, UserHandle.CURRENT);
        builder
            .setContentTitle(title)
            .setContentText(title2)
            .setSmallIcon(id)
            .setContentIntent(pendingIntent)
            .setAutoCancel(autocancel);
        Notification notification = builder.build();
        if (!autocancel) {
            notification.flags |= Notification.FLAG_ONGOING_EVENT;
        }
        if (showTickerText) {
            notification.tickerText = title;
        }
        try {
            if (mBinder.isConnected() && !mBinder.isConnectionOngoing()) {
                mNotificationManager.notifyAsUser(null, id, notification, UserHandle.ALL);
                setForeGround(true, notification, id);
                if (DBG) Log.d(TAG, "NvwfdService is running in foreground");
            }
            mNotificationManager.notifyAsUser(null, id, notification, UserHandle.ALL);
        } catch (Exception ex) {}
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
    }

    private void displayMiracastStatusNotification(int type, String sinkName, boolean autocancel, boolean showTickerText) {
        String message = null;
        int id = -1;

        switch (type) {
            case NOTIF_WARNING_CONN_FAILED :
                message = mContext.getResources().getString(R.string.miracast_connection_status_warning_conn_failed);
                id = R.drawable.ic_miracast_warning;
                break;
            case NOTIF_WARNING_CONN_LOST :
                message = mContext.getResources().getString(R.string.miracast_connection_status_warning_conn_lost);
                id = R.drawable.ic_miracast_warning;
                break;
            case NOTIF_WARNING_AUTHENTIFICATION_FAILED :
                message = mContext.getResources().getString(R.string.miracast_connection_status_warning_auth_failed);
                id = R.drawable.ic_miracast_warning;
                break;
            case NOTIF_SINK_SEARCH :
                message = mContext.getResources().getString(R.string.miracast_connection_status_discovery);
                id = R.drawable.ic_miracast_ready;
                break;
            case NOTIF_CONNECTION_PROGRESS :
                message = mContext.getResources().getString(R.string.miracast_connection_status_progress);
                id = R.drawable.ic_miracast_ready;
                break;
            case NOTIF_STREAMING_ON :
                message = mContext.getResources().getString(R.string.miracast_connection_status_connected);
                id = R.drawable.ic_miracast_connected;
                break;
            default :
                Log.e(TAG, "Invalid notification type !");
                break;
        }
        String message1 = mContext.getResources().getString(R.string.miracast_access_settings_title);
        displayNotification(mMiracastSettingsIntent, id, message + sinkName, message1, autocancel, showTickerText);
    }

    private void displayHDCPNotification(int id, String connectedsink) {
        String titleText = String.format(getString(R.string.miracast_hdcp_title, connectedsink));
        String notificationText = mContext.getResources().getString(R.string.miracast_hdcp_failure_text);

        Notification notification = new Notification.Builder(mContext)
            .setContentTitle(titleText)
            .setContentText(notificationText)
            .setSmallIcon(id)
            .setTicker(titleText)
            .setAutoCancel(true)
            .build();
        mNotificationManager.notifyAsUser(null, id, notification, UserHandle.ALL);
    }

    void cancelNotification(int id) {
        mNotificationManager.cancelAll();
    }

    private void displayHDMINotification(int title, int text) {
        String titleText = mContext.getResources().getString(title);
        String notificationText = mContext.getResources().getString(text);

        Notification notification = new Notification.Builder(mContext)
            .setContentTitle(titleText)
            .setContentText(notificationText)
            .setSmallIcon(R.drawable.ic_hdmi_3d)
            .setTicker(notificationText)
            .setAutoCancel(true)
            .build();
        mNotificationManager.notifyAsUser(null, R.drawable.ic_hdmi_3d, notification, UserHandle.ALL);
    }

    private void doRequiredInSettingsLocation(Context context, boolean bMiracastConnected) {
        if (DBG) Log.d(TAG, "doRequiredInSettingsLocation:connected:" + bMiracastConnected);
        try {
            if (context != null) {
                final ContentResolver cr = context.getContentResolver();
                int bCurVal = Secure.getInt(cr, Secure.LOCATION_MODE);
                if (bMiracastConnected && (bCurVal != Secure.LOCATION_MODE_OFF) && (bCurVal != Secure.LOCATION_MODE_SENSORS_ONLY)) {
                    // off: wifi or mobile networks based GPS
                    Secure.putInt(cr, Secure.LOCATION_MODE, Secure.LOCATION_MODE_SENSORS_ONLY);
                }
            } else {
                Log.e(TAG, "doRequiredInSettingsLocation: ERROR: NULL Ctx");
            }
        } catch(Exception e) {
            Log.e(TAG, e.toString());
        }
        if (DBG) Log.d(TAG, "doRequiredInSettingsLocation:connected:" + bMiracastConnected);
    }
}
