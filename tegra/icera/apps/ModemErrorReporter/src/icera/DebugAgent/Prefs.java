// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class Prefs {

    public static final String PREFS_NAME = "AppPrefs";

    public static final String MDM_LOGGING_KEY = "logging";

    public static final String AUTOLAUNCH_KEY = "launch";

    public static final String AUTOSAVE_KEY = "autoSave";

    public static final String COMPRESS_BEANIE_KEY = "compressbeanie";
    
    public static final String DEFERRED_LOGGING_KEY = "deferredlogging";

    public static final String BEANIE_VERSION_KEY = "beanieversion";

    public static final String BEANIE_PERMISSION_KEY = "beaniepermission";

    public static final String LOGCAT_KEY = "logcat";

    public static final String LOGCAT_ALL_KEY = "logcatall";
    
    public static final String LOGCAT_MAIN_KEY = "logcatmain";

    public static final String LOGCAT_SYSTEM_KEY = "logcatsystem";

    public static final String LOGCAT_EVENTS_KEY = "logcatevents";

    public static final String LOGCAT_RADIO_KEY = "logcatradio";
    
    public static final String NETWORK_LOST_KEY = "networklost";
    
    public static final String UNRECOVER_NETWORK_LOST_KEY = "unrecovernetworklost";

    public static final String DATA_STALL_KEY = "datastall";

    public static final String SIM_LOST_KEY = "simlost";

    public static final String AT_TIMEOUT_KEY = "attimeout";

    public static final String CALL_DROP_KEY = "calldrop";

    public static final String APP_CRASH_KEY = "appcrash";

    public static final String MODEM_CRASH_KEY = "modemcrash";

    public static final String LOGGING_ON_SDCARD_KEY = "SDcard";

    public static final String LOGGING_POSSIBLE_KEY = "logging_poss";

    public static final String ERROR_KEY = "error";

    public static final String FULL_COREDUMP_KEY = "full_coredump";
    
    public static final String COREDUMP_AUTOSAVING_KEY = "coredump_autosaving";

    public static final String COREDUMP_KEY = "coredump";

    public static final String ISSUE_TIME_KEY = "issue_time";

    public static final String NO_LIMIT_KEY = "no_limit";

    public static final String LOGGING_SIZE_KEY = "logging_size";

    public static final String LOG_SIZE_POSITION_KEY = "LogSizePos";
    
    public static final String DEFERRED_TIMEOUT_KEY = "DeferredTimeout";
    
    public static final String DEFERRED_WATERMARK_KEY = "DeferredWatermark";

    public static final String USER_MARKER_STRING = "usermarker";

    public static final String ERROR_NO_CRASH = "NO_CRASH";

    public static final String ERROR_CRASH_MODEM = "NOTIFY_CRASH_MODEM";

    public static final String ERROR_CRASH_RIL = "NOTIFY_CRASH_RIL";

    public static final String ERROR_CRASH_KERNEL = "NOTIFY_CRASH_KERNEL";

    public static final String KERNEL_LOGGING_KEY = "kernel";

    public static final String BUGREPORT_KEY = "bugreport";
    
    public static final String TCPDUMP_KEY = "tcpdump";

    public static final String LASTKMSG_KEY = "lastkmsg";

    // private boolean modemLoggingActive;
    // private boolean logcatLoggingActive;
    // private boolean autoLaunchActive;
    // private boolean autoSavingActive;
    // private boolean loggingOnSDCard;

    private SharedPreferences mSharedPrefs;

    public Prefs(Context context) {
        mSharedPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    private String getString(String key, String def) {
        String s = mSharedPrefs.getString(key, def);
        return s;
    }

    private void setString(String key, String val) {
        Editor e = mSharedPrefs.edit();
        e.putString(key, val);
        e.commit();
    }

    private boolean getBoolean(String key, boolean def) {
        boolean b = mSharedPrefs.getBoolean(key, def);
        return b;
    }

    private void setBoolean(String key, boolean val) {
        Editor e = mSharedPrefs.edit();
        e.putBoolean(key, val);
        e.commit();
    }

    private int getInt(String key, int def) {
        int i = mSharedPrefs.getInt(key, def);
        return i;
    }

    private void setInt(String key, int val) {
        Editor e = mSharedPrefs.edit();
        e.putInt(key, val);
        e.commit();
    }

    private long getLong(String key, long def) {
        long l = mSharedPrefs.getLong(key, def);
        return l;
    }

    private void setLong(String key, long val) {
        Editor e = mSharedPrefs.edit();
        e.putLong(key, val);
        e.commit();
    }

    public boolean isModemLoggingEnabled() {
        return getBoolean(MDM_LOGGING_KEY, true);
    }

    public void setModemLoggingEnabled(boolean modemLoggingActive) {
        setBoolean(MDM_LOGGING_KEY, modemLoggingActive);
    }

    public boolean isKernelLoggingEnabled() {
        return getBoolean(KERNEL_LOGGING_KEY, false);
    }

    public void setKernelLoggingEnabled(boolean kernelLoggingActive) {
        setBoolean(KERNEL_LOGGING_KEY, kernelLoggingActive);
    }

    public boolean isBugreportEnabled() {
        return getBoolean(BUGREPORT_KEY, true);
    }

    public void setBugreportEnabled(boolean bugreportActive) {
        setBoolean(BUGREPORT_KEY, bugreportActive);
    }
    
    public boolean isTCPDumpEnabled() {
        return getBoolean(TCPDUMP_KEY, false);
    }

    public void setTCPDumpEnabled(boolean tcpDumpActive) {
        setBoolean(TCPDUMP_KEY, tcpDumpActive);
    }

    public boolean isLastKmsgEnabled() {
        return getBoolean(LASTKMSG_KEY, true);
    }

    public void setLastKmsgEnabled(boolean laskmsgActive) {
        setBoolean(LASTKMSG_KEY, laskmsgActive);
    }

    public boolean isLogcatEnabled() {
        return getBoolean(LOGCAT_KEY, true);
    }

    public void setLogcatEnabled(boolean logcatLoggingActive) {
        setBoolean(LOGCAT_KEY, logcatLoggingActive);
    }
    
    public boolean isLogcatAllEnabled() {
        return getBoolean(LOGCAT_ALL_KEY, false);
    }

    public void setLogcatAllEnabled(boolean logcatLoggingActive) {
        setBoolean(LOGCAT_ALL_KEY, logcatLoggingActive);
    }

    public boolean isLogcatMainEnabled() {
        return getBoolean(LOGCAT_MAIN_KEY, true);
    }

    public void setLogcatMainEnabled(boolean logcatLoggingActive) {
        setBoolean(LOGCAT_MAIN_KEY, logcatLoggingActive);
    }

    public boolean isLogcatSystemEnabled() {
        return getBoolean(LOGCAT_SYSTEM_KEY, true);
    }

    public void setLogcatSystemEnabled(boolean logcatLoggingActive) {
        setBoolean(LOGCAT_SYSTEM_KEY, logcatLoggingActive);
    }

    public boolean isLogcatEventsEnabled() {
        return getBoolean(LOGCAT_EVENTS_KEY, true);
    }

    public void setLogcatEventsEnabled(boolean logcatLoggingActive) {
        setBoolean(LOGCAT_EVENTS_KEY, logcatLoggingActive);
    }

    public boolean isLogcatRadioEnabled() {
        return getBoolean(LOGCAT_RADIO_KEY, true);
    }

    public void setLogcatRadioEnabled(boolean logcatLoggingActive) {
        setBoolean(LOGCAT_RADIO_KEY, logcatLoggingActive);
    }

    public boolean isNetworkLostEnabled() {
        return getBoolean(NETWORK_LOST_KEY, false);
    }

    public void setNetworkLostEnabled(boolean NetworkLostActive) {
        setBoolean(NETWORK_LOST_KEY, NetworkLostActive);
    }

    public boolean isUnrecoverNetworkLostEnabled() {
        return getBoolean(UNRECOVER_NETWORK_LOST_KEY, true);
    }

    public void setUnrecoverNetworkLostEnabled(boolean isEnable) {
        setBoolean(UNRECOVER_NETWORK_LOST_KEY, isEnable);
    }

    public boolean isDataStallEnabled() {
        return getBoolean(DATA_STALL_KEY, true);
    }

    public void setDataStallEnabled(boolean dataStallActive) {
        setBoolean(DATA_STALL_KEY, dataStallActive);
    }

    public boolean isSimLostEnabled() {
        return getBoolean(SIM_LOST_KEY, true);
    }

    public void setSimLostEnabled(boolean simLostActive) {
        setBoolean(SIM_LOST_KEY, simLostActive);
    }

    public boolean isAtTimeoutEnabled() {
        return getBoolean(AT_TIMEOUT_KEY, true);
    }

    public void setAtTimeoutEnabled(boolean atTimeoutActive) {
        setBoolean(AT_TIMEOUT_KEY, atTimeoutActive);
    }

    public boolean isCallDropEnabled() {
        return getBoolean(CALL_DROP_KEY, true);
    }

    public void setCallDropEnabled(boolean callDropActive) {
        setBoolean(CALL_DROP_KEY, callDropActive);
    }

    public boolean isAppCrashEnabled() {
        return getBoolean(APP_CRASH_KEY, false);
    }

    public void setAppCrashEnabled(boolean appCrashActive) {
        setBoolean(APP_CRASH_KEY, appCrashActive);
    }

    public boolean isAutoLaunchEnabled() {
        return getBoolean(AUTOLAUNCH_KEY, false);
    }

    public void setAutoLaunchEnabled(boolean autoLaunchActive) {
        setBoolean(AUTOLAUNCH_KEY, autoLaunchActive);
    }

    public boolean isAutoSavingEnabled() {
        return getBoolean(AUTOSAVE_KEY, true);
    }

    public void setAutoSavingEnabled(boolean autoSavingActive) {
        setBoolean(AUTOSAVE_KEY, autoSavingActive);
    }

    public boolean isCompressBeanieEnabled() {
        return getBoolean(COMPRESS_BEANIE_KEY, true);
    }

    public void setCompressBeanieEnabled(boolean compressBeanieActive) {
        setBoolean(COMPRESS_BEANIE_KEY, compressBeanieActive);
    }
    
    public boolean isDeferredLoggingEnabled() {
        return getBoolean(DEFERRED_LOGGING_KEY, true);
    }

    public void setDeferredLoggingEnabled(boolean deferred) {
        setBoolean(DEFERRED_LOGGING_KEY, deferred);
    }

    public boolean isBeanieVersionOld() {
        return getBoolean(BEANIE_VERSION_KEY, false);
    }

    public void setBeanieVersionOld(boolean old) {
        setBoolean(BEANIE_VERSION_KEY, old);
    }

    public boolean isBeaniePermission() {
        return getBoolean(BEANIE_PERMISSION_KEY, true);
    }

    public void setBeaniePermission(boolean perm) {
        setBoolean(BEANIE_PERMISSION_KEY, perm);
    }

    public boolean isLoggingOnSDCard() {
        return getBoolean(LOGGING_ON_SDCARD_KEY, false);
    }

    public void setLoggingOnSDCard(boolean loggingOnSDCard) {
        setBoolean(LOGGING_ON_SDCARD_KEY, loggingOnSDCard);
    }

    public long getLoggingSize() {
        return getLong(LOGGING_SIZE_KEY, 500);
    }

    public void setLoggingSize(long size) {
        setLong(LOGGING_SIZE_KEY, size);
    }

    public boolean getNoLimit() {
        return getBoolean(NO_LIMIT_KEY, false);
    }

    public void setNoLimit(boolean b) {
        setBoolean(NO_LIMIT_KEY, b);
    }

    public int getLogSizePos() {
        return getInt(LOG_SIZE_POSITION_KEY, 2);
    }

    public void setLogSizePos(int pos) {
        setInt(LOG_SIZE_POSITION_KEY, pos);
    }

    public int getDeferredTimeoutPos() {
        return getInt(DEFERRED_TIMEOUT_KEY, 60);
    }

    public void setDeferredTimeoutPos(int pos) {
        setInt(DEFERRED_TIMEOUT_KEY, pos);
    }
    
    public int getDeferredWatermarkPos() {
        return getInt(DEFERRED_WATERMARK_KEY, 60);
    }

    public void setDeferredWatermarkPos(int pos) {
        setInt(DEFERRED_WATERMARK_KEY, pos);
    }

    public String getUserMarkerString() {
        return getString(USER_MARKER_STRING, "tester_marker");
    }

    public void setUserMarkerString(String marker) {
        setString(USER_MARKER_STRING, marker);
    }
}
