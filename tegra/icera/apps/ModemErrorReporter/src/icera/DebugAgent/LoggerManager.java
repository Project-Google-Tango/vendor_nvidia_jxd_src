// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import icera.DebugAgent.LogUtil.StoreProgressListener;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.File;
import java.io.FileOutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.EnumMap;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.TimeUnit;

import android.os.Environment;
import android.util.Log;

/*
 * Manage the different loggers and dumpers
 * Maintain the synchronization between preferences and loggers/dumpers status
 * 
 */
public class LoggerManager {

    private static final String TAG = "LoggerManager";

    private BugReportService mService = null;

    private Prefs mPrefs = null;

    private boolean mRilLoggingEnabled = true;

    enum LoggerType {
        MODEM, MODEMZIP, LOGCAT, KERNEL, LCMAIN, LCSYSTEM, LCEVENTS, LCRADIO
    }

    private EnumMap<LoggerType, Logger> mActiveLoggers = new EnumMap<LoggerType, Logger>(
            LoggerType.class);

    private List<LoggerType> mPrevLoggers = new ArrayList<LoggerType>(); // Hold
                                                                         // the
                                                                         // previous
                                                                         // loggers
                                                                         // when
                                                                         // updating
                                                                         // prefs

    private List<LogSet> mStoreList = Collections.synchronizedList(new ArrayList<LogSet>());

    public LoggerManager(BugReportService ctx) {

        mService = ctx;
        mPrefs = new Prefs(mService);
        mPrefs.setLoggingOnSDCard(LogUtil.isExternStorageUsable());
        LoggerToolInfo info = supportDeferredFeature();
        switch (info) {
            case VERSION_NEW:
                Log.d(TAG, "LoggerToolInfo.VERSION_NEW");
                mPrefs.setBeanieVersionOld(false);
                mPrefs.setBeaniePermission(true);
                break;
            case VERSION_OLD:
                Log.d(TAG, "LoggerToolInfo.VERSION_OLD");
                mPrefs.setBeanieVersionOld(true);
                mPrefs.setBeaniePermission(true);
                break;
            case PERMISSION_NOT_ENOUGH:
                Log.d(TAG, "LoggerToolInfo.PERMISSION_NOT_ENOUGH");
                mPrefs.setBeaniePermission(false);
                break;
        }

        SaveLogCache();
        cleanExistingLogs();
        enableFullCoredump();
    }

    private void cleanExistingLogs() {

        cleanLoggingDirectory(mService.getFilesDir());

        File tmpLogDir = new File(Environment.getExternalStorageDirectory() + File.separator
                + LogUtil.DEFAULT_LOGGING_DIRECTORY_SD);
        if (tmpLogDir != null && tmpLogDir.exists())
            cleanLoggingDirectory(tmpLogDir);

    }

    public static void cleanLoggingDirectory(File dir) {

        String cmd = "rm -r " + dir.getAbsolutePath() + File.separator + "*";
        if (dir.list() != null) {
            if ((dir.list()).length > 0) {
                runShellCmd(cmd);
            }
        }
    }

    public static void moveLogCacheFiles(String src, String dest) {
        File srcF = new File(src);
        if (srcF.isFile()){
            File destF = new File(dest);
            if (!destF.exists())
                destF.mkdirs();
            srcF.renameTo(new File(dest + File.separator + srcF.getName()));
        }
        else if (srcF.isDirectory()){
            for (File chld: srcF.listFiles()){
                moveLogCacheFiles(chld.getAbsolutePath(), dest);
            }
        }
        else {
            return;
        }
    }

    public void SaveLogCache(){
        boolean sdUsable = mPrefs.isLoggingOnSDCard();
        String logDir = LogUtil.LOG_DATE_FORMAT.format(new Date()) + "_REBOOT";
        if (sdUsable){
            logDir = Environment.getExternalStorageDirectory() + File.separator
                    + LogUtil.LOG_STORE_HOME + File.separator + logDir;
            moveLogCacheFiles(Environment.getExternalStorageDirectory() + File.separator
                    + LogUtil.DEFAULT_LOGGING_DIRECTORY_SD, logDir);
        }
        else {
            logDir = "/data/rfs/data/debug/"
                    + LogUtil.LOG_STORE_HOME + File.separator + logDir;
            moveLogCacheFiles(mService.getFilesDir().getAbsolutePath(),
                    logDir);
        }
        if (new File(logDir).exists()) {
            String kmsg_cmd = "cat /proc/last_kmsg > " + logDir + File.separator + "last_kmsg.txt";
            runShellCmd(kmsg_cmd);
        }
    }

    /*
     * Enable full coredump by command the at command to modem
     */
    public void enableFullCoredump() {
        String cmd = "echo \"AT%IFULLCOREDUMP=1\r\" > " + SaveLogs.getATPort();
        runShellCmd(cmd);
    }
    static public void runShellCmd(String cmd) {
        try {
            Runtime.getRuntime().exec(new String[] {
                            "/system/bin/sh", "-c", cmd
                    });
            Log.d(TAG, "Execute the shell command:" + cmd);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /*
     * Start loggers according to user preferences
     */
    public void startUserLoggers() {

        if (mPrefs.isModemLoggingEnabled()) {
            ModemLogger mdmLogger = null;

            if (checkAvailableSpace()) {
                if (mPrefs.isCompressBeanieEnabled()) {
                    mdmLogger = new ModemLoggerZip(mService);
                } else {
                    mdmLogger = new ModemLogger(mService);
                }
                if (!mdmLogger.start()) {
                    mPrefs.setModemLoggingEnabled(false);
                    mService.notifyMessage(BugReportService.NotificationType.NOTIFY_MODEM_LOGGING_FAILED);
                    Log.d(TAG, "Failed to start modem logging");
                } else {
                    mActiveLoggers.put(mPrefs.isCompressBeanieEnabled() ? LoggerType.MODEMZIP
                            : LoggerType.MODEM, mdmLogger);
                    Log.d(TAG, "Modem logging started");
                }
            } else
                mPrefs.setModemLoggingEnabled(false);
        }

        // if (mPrefs.isLogcatEnabled()) {
        // Logcat logcat = new Logcat(mService);
        // if (!logcat.start()) {
        // mPrefs.setLogcatEnabled(false);
        // } else {
        // mActiveLoggers.put(LoggerType.LOGCAT, logcat);
        // }
        // }
        //
        // if (mPrefs.isKernelLoggingEnabled()) {
        // KernelLogger klogger = new KernelLogger(mService);
        // if (!klogger.start()) {
        // mPrefs.setKernelLoggingEnabled(false);
        // } else {
        // mActiveLoggers.put(LoggerType.KERNEL, klogger);
        // }
        // }
        if (mPrefs.isLogcatEnabled() && mPrefs.isLogcatAllEnabled()) {
            Logcat logcat = new Logcat(mService);
            if (!logcat.start()) {
                mPrefs.setLogcatAllEnabled(false);
            } else {
                mActiveLoggers.put(LoggerType.LOGCAT, logcat);
            }
        }

        if (mPrefs.isLogcatEnabled() && mPrefs.isLogcatMainEnabled()) {
            LogcatMain logcat = new LogcatMain(mService);
            if (!logcat.start()) {
                mPrefs.setLogcatMainEnabled(false);
            } else {
                mActiveLoggers.put(LoggerType.LCMAIN, logcat);
            }
        }

        if (mPrefs.isLogcatEnabled() && mPrefs.isLogcatSystemEnabled()) {
            LogcatSystem logcat = new LogcatSystem(mService);
            if (!logcat.start()) {
                mPrefs.setLogcatSystemEnabled(false);
            } else {
                mActiveLoggers.put(LoggerType.LCSYSTEM, logcat);
            }
        }

        if (mPrefs.isLogcatEnabled() && mPrefs.isLogcatEventsEnabled()) {
            LogcatEvents logcat = new LogcatEvents(mService);
            if (!logcat.start()) {
                mPrefs.setLogcatEventsEnabled(false);
            } else {
                mActiveLoggers.put(LoggerType.LCEVENTS, logcat);
            }
        }

        if (mPrefs.isLogcatEnabled() && mPrefs.isLogcatRadioEnabled()) {
            LogcatRadio logcat = new LogcatRadio(mService);
            if (!logcat.start()) {
                mPrefs.setLogcatRadioEnabled(false);
            } else {
                mActiveLoggers.put(LoggerType.LCRADIO, logcat);
            }
        }
    }

    public boolean startLogger(LoggerType loggerType) {

        boolean success;
        Logger logger = null;

        if (mActiveLoggers.get(loggerType) != null) {
            Log.e(TAG, "Logger is already running. please stop it first.");
            return false;
        }

        if (loggerType == LoggerType.MODEM || loggerType == LoggerType.MODEMZIP) {
            if (!checkAvailableSpace()) {
                mPrefs.setModemLoggingEnabled(false);
                return false;
            }
        }

        switch (loggerType) {
            case MODEM:
                logger = new ModemLogger(mService);
                break;

            case MODEMZIP:
                logger = new ModemLoggerZip(mService);
                break;

            case LOGCAT:
                logger = new Logcat(mService);
                break;
            case LCMAIN:
                logger = new LogcatMain(mService);
                break;
            case LCSYSTEM:
                logger = new LogcatSystem(mService);
                break;
            case LCEVENTS:
                logger = new LogcatEvents(mService);
                break;
            case LCRADIO:
                logger = new LogcatRadio(mService);
                break;
            case KERNEL:
                logger = new KernelLogger(mService);
                break;

            default:
                break;
        }

        if (logger.start()) {
            mActiveLoggers.put(loggerType, logger);
            switch (loggerType) {
                case MODEM:
                case MODEMZIP:
                    mPrefs.setModemLoggingEnabled(true);
                    break;
                case LOGCAT:
                    mPrefs.setLogcatAllEnabled(true);
                    break;
                case LCMAIN:
                    mPrefs.setLogcatMainEnabled(true);
                    break;
                case LCSYSTEM:
                    mPrefs.setLogcatSystemEnabled(true);
                    break;
                case LCEVENTS:
                    mPrefs.setLogcatEventsEnabled(true);
                    break;
                case LCRADIO:
                    mPrefs.setLogcatRadioEnabled(true);
                    break;
                case KERNEL:
                    mPrefs.setKernelLoggingEnabled(true);
                    break;
            }
            success = true;
        } else {
            Log.e(TAG, "Failed to start the logger");
            switch (loggerType) {
                case MODEM:
                case MODEMZIP:
                    mService.notifyMessage(BugReportService.NotificationType.NOTIFY_MODEM_LOGGING_FAILED);
                    mPrefs.setModemLoggingEnabled(false);
                    break;
                case LOGCAT:
                    mPrefs.setLogcatAllEnabled(false);
                    break;
                case LCMAIN:
                    mPrefs.setLogcatMainEnabled(false);
                    break;
                case LCSYSTEM:
                    mPrefs.setLogcatSystemEnabled(false);
                    break;
                case LCEVENTS:
                    mPrefs.setLogcatEventsEnabled(false);
                    break;
                case LCRADIO:
                    mPrefs.setLogcatRadioEnabled(false);
                    break;

                case KERNEL:
                    mPrefs.setKernelLoggingEnabled(false);
                    break;
            }
            success = false;
        }

        return success;
    }

    public void stopLogger(LoggerType loggerType, boolean store, LogSet ls, boolean updatePref,
            boolean restart) {

        Logger logger = mActiveLoggers.get(loggerType);

        if (logger != null) {

            if (store) {
                logger.stop();
                if (ls != null)
                    ls.addLog(logger);
            } else {
                logger.stopAndClean();
            }

            mActiveLoggers.remove(loggerType);

            if (updatePref) {
                switch (loggerType) {
                    case MODEM:
                    case MODEMZIP:
                        mPrefs.setModemLoggingEnabled(false);
                        break;
                    case LOGCAT:
                        if (mPrefs.isLogcatEnabled())
                        mPrefs.setLogcatAllEnabled(false);
                        break;
                    case LCMAIN:
                        if (mPrefs.isLogcatEnabled())
                        mPrefs.setLogcatMainEnabled(false);
                        break;
                    case LCSYSTEM:
                        if (mPrefs.isLogcatEnabled())
                        mPrefs.setLogcatSystemEnabled(false);
                        break;
                    case LCEVENTS:
                        if (mPrefs.isLogcatEnabled())
                        mPrefs.setLogcatEventsEnabled(false);
                        break;
                    case LCRADIO:
                        if (mPrefs.isLogcatEnabled())
                        mPrefs.setLogcatRadioEnabled(false);
                        break;

                    case KERNEL:
                        mPrefs.setKernelLoggingEnabled(false);
                        break;
                }
            }

            if (restart) {
                startLogger(loggerType);
            }

            if (updatePref && !restart && store) // Loggers setting will be lost in this
                                        // case. Hold it for later use.
                mPrevLoggers.add(loggerType);
        }
    }

    public boolean startMdmLogging() {

        return startLogger(mPrefs.isCompressBeanieEnabled() ? LoggerType.MODEMZIP
                : LoggerType.MODEM);
    }

    /*
     * stop modem logging and clean the logging files
     */
    public void stopMdmLogging() {

        stopLogger(mPrefs.isCompressBeanieEnabled() ? LoggerType.MODEMZIP : LoggerType.MODEM,
                false, null, true, false);
    }

    public boolean startKernelLogging() {
        return startLogger(LoggerType.KERNEL);
    }

    public void stopKernelLogging() {
        stopLogger(LoggerType.KERNEL, false, null, true, false);
    }

    public boolean startLogcat() {

        return startLogger(LoggerType.LOGCAT);
    }

    /*
     * stop logcat and clean the logging files
     */
    public void stopLogcat() {

        stopLogger(LoggerType.LOGCAT, false, null, true, false);
    }

    public boolean startLogcatMain() {

        return startLogger(LoggerType.LCMAIN);
    }

    public void stopLogcatMain() {

        stopLogger(LoggerType.LCMAIN, false, null, true, false);
    }

    public boolean startLogcatSystem() {

        return startLogger(LoggerType.LCSYSTEM);
    }

    public void stopLogcatSystem() {

        stopLogger(LoggerType.LCSYSTEM, false, null, true, false);
    }

    public boolean startLogcatEvents() {

        return startLogger(LoggerType.LCEVENTS);
    }

    public void stopLogcatEvents() {

        stopLogger(LoggerType.LCEVENTS, false, null, true, false);
    }

    public boolean startLogcatRadio() {

        return startLogger(LoggerType.LCRADIO);
    }

    public void stopLogcatRadio() {

        stopLogger(LoggerType.LCRADIO, false, null, true, false);
    }

    public boolean runLogcatRadio() {
        
        if (mActiveLoggers.get(LoggerType.LCRADIO) != null) {
            mRilLoggingEnabled = true;
            return true;
        }
        mRilLoggingEnabled = !startLogger(LoggerType.LCRADIO);
        return !mRilLoggingEnabled;
    }

    public boolean cancelLogcatRadio() {
        if (!mRilLoggingEnabled){
            stopLogger(LoggerType.LCRADIO, false, null, true, false);
            mRilLoggingEnabled = true;
        }
        try {
            Runtime.getRuntime().exec(new String[] {
                            "logcat", "-b", "radio", "-c"
                    });
        } catch (IOException e) {
            e.printStackTrace();
        }
        return mRilLoggingEnabled;
    }

    public String getRilLog(){
        String path = null;
        runLogcatRadio();
        LogcatRadio rilLogger = (LogcatRadio)mActiveLoggers.get(LoggerType.LCRADIO);
        if (rilLogger != null) {
            path = rilLogger.getLogAbsolutePath();
        }
        return path;
    }

    private LogSet addAllLoggersToLogSet(LogSet logset, boolean restart) {

        LogSet ls = logset;
        if (ls == null) {
            ls = new LogSet();
            ls.setIssueTime(new Date());
            ls.setReason(LogReason.NO_CRASH);
        }

        if (!mActiveLoggers.isEmpty()) {
            if (!restart)
                mPrevLoggers.clear();

            for (LoggerType lt : mActiveLoggers.keySet()) {
                stopLogger(lt, true, ls, true, restart);
            }

        }

        if (mPrefs.isLogcatEnabled() && mPrefs.isLastKmsgEnabled()) {
            LogUtil kdumper = new LastKmsgDumper(mService);
            if (!kdumper.start()) {
                Log.e(TAG, "LastKmsg dump failed!");
                kdumper.deleteLogs();
            } else
                ls.addLog(kdumper);
        }

        if (mPrefs.isLogcatEnabled() && mPrefs.isBugreportEnabled()) {
            LogUtil bugreport = new BugreportDumper(mService);
            if (!bugreport.start()) {
                Log.e(TAG, "Bugreport dump failed!");
                bugreport.deleteLogs();
            } else
                ls.addLog(bugreport);
        }
        return ls;
    }

    public void stopAllToStore(boolean restart) {

        LogSet ls = addAllLoggersToLogSet(null, restart);

        if (ls != null)
            mStoreList.add(ls);

    }

    /*
     * stop all loggers without storing the logging files
     */
    public void stopAllAndClean(boolean updatePref, boolean restart) {

        if (!mActiveLoggers.isEmpty()) {
            for (LoggerType lt : mActiveLoggers.keySet()) {
                stopLogger(lt, false, null, updatePref, restart);
            }
        }
    }

    /*
     * restart all running loggers without storing logging files
     */
    public void restartLogging() {

        stopAllAndClean(false, true);
    }

    /*
     * resume the previous loggers
     */
    public void resumeLoggers() {
        LoggerType lt;

        for (Iterator<LoggerType> it = mPrevLoggers.iterator(); it.hasNext();) {
            lt = it.next();
            it.remove();
            if (startLogger(lt))
                Log.d(TAG, lt.toString() + " resumed");
            else
                Log.e(TAG, lt.toString() + "could not resume");
        }
    }

    public void storeLogs(final StoreProgressListener spl) {

        Log.d(TAG, "Num. of Logsets to store: " + mStoreList.size());

        LogSet ls;
        for (Iterator<LogSet> it = mStoreList.iterator(); it.hasNext();) {

            synchronized (this) {
                ls = it.next();
                it.remove();
            }

            ls.store(spl);
            Log.d(TAG, "A logset stored: " + LogUtil.LOG_DATE_FORMAT.format(ls.mIssueTime));
        }
    }

    /*
     * Store the log files of the loggers in the StoreList
     */
    public void storeLogs(String destDirname, final StoreProgressListener spl) {

        int storeListSize = mStoreList.size();
        if (storeListSize == 0)
            return;

        Log.d(TAG, "Num. of Logsets to store: " + mStoreList.size());

        LogSet ls;

        // Only the last logset needs to be stored to destDirname
        synchronized (this) {
            ls = mStoreList.remove(mStoreList.size() - 1);
        }
        ls.store(destDirname, spl);
        Log.d(TAG, "A logset stored: " + destDirname /*
                                                      * LogUtil.LOG_DATE_FORMAT.
                                                      * format(ls.mIssueTime)
                                                      */);

        storeLogs(spl);
    }

    public LogReason handleModemStateChange() {

        LogReason reason;

        LogSet ls = new LogSet();
        ls.setIssueTime(new Date());

        Coredumper cdmp = new Coredumper(mService);
        cdmp.setRilLogPath(getRilLog());
        // Check if modem has crashed
        cdmp.start();
        cdmp.updateLogList();
        Log.d(TAG, "Modem crash detected");
        ls.addLog(cdmp);

        // add all active loggers
        addAllLoggersToLogSet(ls, false);
        reason = LogReason.CRASH_MODEM;

        ls.setReason(reason);

        // synchronized (this) {
        mStoreList.add(ls);

        Log.d(TAG, "Num of log set in store list: " + mStoreList.size());

        return reason;
    }

    public LogReason handleRilStateChange() {

        LogReason reason;
        Logcat lc = new Logcat(mService);
        // check if the RIL has restarted due to a modem crash
        if (lc.getCrash()) {
            Log.d(TAG, "RIL crash detected");

            LogSet ls = new LogSet();
            ls.setIssueTime(new Date());

            // clear the RIl restart log to avoid a second RIL_Init detection
            cancelLogcatRadio();

            // add all active loggers
            addAllLoggersToLogSet(ls, false);
            reason = LogReason.CRASH_RIL;

            ls.setReason(reason);

            // synchronized (this) {
            mStoreList.add(ls);

            Log.d(TAG, "Num of log set in store list: " + mStoreList.size());

        } else {
            Log.d(TAG, "No crash detected");
            reason = LogReason.NO_CRASH;
        }

        return reason;
    }

    public LogReason handleAppCrash() {

        LogReason reason = null;
        if (mPrefs.isAppCrashEnabled() && mPrefs.isAutoSavingEnabled());
        else
            return reason;
        LogSet ls = new LogSet();
        ls.setIssueTime(new Date());
        ApCrashLogger AppLogger = new ApCrashLogger(mService);
        AppLogger.start();
        if (AppLogger.getLogSize() != 0){
            Log.d(TAG, "App crash detected!");
            reason = LogReason.APP_CRASH;
            ls.addLog(AppLogger);
            ls.setReason(LogReason.APP_CRASH);
        }
        else {
            AppLogger.deleteLogs();
        }
        mStoreList.add(ls);
        //addAllLoggersToLogSet(ls, false);

        return reason;
    }

    public void saveComment(String comment, String logName) {

        FileOutputStream out = null;
        try {
            ModemLogger mdmLogger = (ModemLogger)mActiveLoggers.get(mPrefs
                    .isCompressBeanieEnabled() ? LoggerType.MODEMZIP : LoggerType.MODEM);
            if (mdmLogger == null)
                return;

            out = new FileOutputStream(mdmLogger.mLoggingDir.toString() + File.separator
                    + "comment.txt");
            SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSSZ");     
            String date = dateFormat.format(new Date()) + "\r\n";
            out.write(date.getBytes("UTF-8"));
            if (logName != null)
            out.write(logName.getBytes("UTF-8"));
            if (comment != null)
            out.write(comment.getBytes("UTF-8"));
            out.flush();
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                out.close();
                out = null;
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public boolean canStoreOnExternStorage(long sizeNeeded) {

        if (Environment.getExternalStorageDirectory().canWrite()) {
            long freeSpace = Environment.getExternalStorageDirectory().getFreeSpace();
            if (freeSpace != 0 && freeSpace > sizeNeeded) {
                return true;
            }
        }

        return false;
    }

    /*
     * Make sure we have sufficient space for modem logging
     */
    private boolean checkAvailableSpace() {
        long freeSpace;
        boolean success = true;

        if (LogUtil.isExternStorageUsable())
            freeSpace = Environment.getExternalStorageDirectory().getFreeSpace();
        else
            freeSpace = Environment.getDataDirectory().getFreeSpace();
        freeSpace -= LogUtil.FULL_COREDUMP_FILE_SIZE * 3 + SaveLogs.RESERVED_SPACE_SIZE * 1024
                * 1024;

        if (!mPrefs.getNoLimit()) {
            if (freeSpace <= mPrefs.getLoggingSize() * 1024 * 1024)
                success = false;
        } else if ((freeSpace <= (SaveLogs.DEFAULT_IOM_SIZE * 1024 * 1024)
                * SaveLogs.MINIMUM_NUM_OF_IOM)) {
            success = false;
        }

        if (!success) {
            Log.e(TAG, "Not enough space to start modem logging");
            mService.notifyMessage(BugReportService.NotificationType.NOTIFY_INSUFFICIENT_SPACE);
        }

        return success;
    }

    enum LogReason {
        CRASH_MODEM, CRASH_RIL, CRASH_KERNEL, CRASH_NONE, NO_CRASH, APP_CRASH, UNKNOWN
    }

    class LogSet {

        private ArrayList<LogUtil> mLogUtilList = new ArrayList<LogUtil>();

        private Date mIssueTime;

        private LogReason mReason = LogReason.CRASH_NONE;

        public void addLog(LogUtil log) {

            mLogUtilList.add(log);
        }

        public void setIssueTime(Date issueTime) {

            mIssueTime = issueTime;
        }

        public void setReason(LogReason reason) {

            mReason = reason;
        }

        public long getTotalLogSize() {
            long totalSize = 0;

            for (LogUtil l : mLogUtilList) {
                totalSize += l.getLogSize();
            }

            return totalSize;
        }

        public void store(final StoreProgressListener spl) {

            String destDirName = LogUtil.LOG_DATE_FORMAT.format(mIssueTime);

            switch (mReason) {

                case CRASH_MODEM:
                    destDirName += "_MODEM";
                    break;

                case CRASH_RIL:
                    destDirName += "_RIL";
                    break;

                case CRASH_KERNEL:
                    destDirName += "_KERNEL";
                    break;

                case APP_CRASH:
                    destDirName += "_APP";
                    break;

                default:
                    destDirName += "_LOGS";

            }
            store(destDirName, spl);
        }

        public void store(String dirName, final StoreProgressListener spl) {

            long totalLogSize = 0;

            for (LogUtil l : mLogUtilList) {
                totalLogSize += l.getLogSize();
            }

            if (totalLogSize == 0) {
                for (LogUtil l : mLogUtilList) {
                    l.deleteLogs();
                }

                mLogUtilList.clear();
                return;
            }

            Log.d(TAG, "Total log size in this logset: " + totalLogSize);

            String storeRoot;
            boolean onSD;

            if (Environment.getExternalStorageState().contentEquals(Environment.MEDIA_MOUNTED)
                    && Environment.getExternalStorageDirectory().canWrite() /*
                                                                             * canStoreOnExternStorage
                                                                             * (
                                                                             * 0
                                                                             * totalLogSize
                                                                             * )
                                                                             */) {
                storeRoot = Environment.getExternalStorageDirectory().getAbsolutePath()
                        + File.separator + LogUtil.LOG_STORE_HOME;
                onSD = true;
            } else {
                onSD = false;
                storeRoot = LogUtil.DEFAULT_STORE_ROOT;
            }

            if (!new File(storeRoot).exists())
                new File(storeRoot).mkdirs();

            File destDir = new File(storeRoot + File.separator + dirName);
            destDir.mkdirs();

            double accumWeight = 0;
            for (LogUtil l : mLogUtilList) {

                long logSize = l.getLogSize();

                if (logSize != 0) {
                    final double weight = ((double)logSize) / totalLogSize;
                    final double accuWeight = accumWeight * 100;
                    StoreProgressListener thisSPL = new StoreProgressListener() {

                        @Override
                        public void storeProgressUpdate(int progress) {

                            if (spl != null)
                                spl.storeProgressUpdate((int)(progress * weight + accuWeight));

                        }

                    };

                    l.store(destDir, onSD, thisSPL);
                    accumWeight += weight;
                } else
                    l.deleteLogs();
            }

            mLogUtilList.clear();
        }
    }
    enum LoggerToolInfo {
        VERSION_NEW, VERSION_OLD, PERMISSION_NOT_ENOUGH
    }
 
    public LoggerToolInfo supportDeferredFeature() {
        try{
            Process proc = Runtime.getRuntime().exec(new String[]{"sh", "-c", "icera_log_serial_arm 2>&1"});
            BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream()));
            String str;
            while ((str = br.readLine()) != null){
                Log.d(TAG, str);
                if (str.contains("Deferred logging")){
                    proc.destroy();
                    return LoggerToolInfo.VERSION_NEW;
                }
                if (str.contains("Permission denied")){
                    return LoggerToolInfo.PERMISSION_NOT_ENOUGH;
                }
            }
        }
        catch (IOException e){
            e.printStackTrace();
        }
        return LoggerToolInfo.VERSION_OLD;
    }
}
