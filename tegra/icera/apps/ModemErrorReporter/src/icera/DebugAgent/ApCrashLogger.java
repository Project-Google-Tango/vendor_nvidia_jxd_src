// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import android.content.Context;
import android.os.Environment;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.PowerManager.WakeLock;
import android.provider.Settings.Secure;
import android.telephony.TelephonyManager;
import android.util.Log;

public class ApCrashLogger extends LogDumper {

    static final String TAG = "ApCrashLogger";
    private WakeLock mWakeLock = null;
    private String mDumpUsageStat;
    private String mDumpMemInfo;
    private String mDumpDmesg;
    private String mDumpLogcat;
    private String mDumpTrace;
    private String mDumpPanic;
    //private String mDumpRil;
    private String mDeviceInfo = "dumpDeviceInfo";
    private String mZipFile;
    private String mGZipFile;
    private static final String TRACE_TXT     = "/data/anr/traces.txt";
    private static final String PANIC_TXT     = "/data/panicreports";
    private FileUtiles mFileUtiles     = null;
    private PowerManager mPowerManager      = null;

    public ApCrashLogger(Context ctx) {
        super(ctx);
        init();
    }

    @Override
    protected void setLogFileName() {
        mLogFileName = "";
        if (mLoggingDir != null){
            mDumpUsageStat = mLoggingDir + File.separator + "dumpUsage";
            mDumpMemInfo = mLoggingDir + File.separator + "dumpMeminfo";
            mDumpDmesg = mLoggingDir + File.separator + "dumpDmesg";
            mDumpLogcat = mLoggingDir + File.separator + "dumpLogcat";
            mDumpTrace = mLoggingDir + File.separator + "dumpTrace";
            mDumpPanic = mLoggingDir + File.separator + "dumpPainic";
//          mDumpRil = mLoggingDir + "logcat_ril.log";
            mDeviceInfo = mLoggingDir + File.separator + "dumpDeviceInfo";
            mZipFile= mLoggingDir.getParent() + File.separator + "nvlogdump.zip";
            mGZipFile = mLoggingDir.getParent() + File.separator + "nvlogdump.zip.gz";
        }
    }

    @Override
    protected void prepareProgram() {
    }


    @Override
    protected void updateLogList() {
        bugreport();
        addToLogList(new File(mGZipFile));
    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "APPCRASH";
    }

    /*
     * Run the logging program and collect the log files, in one-shot. no 'stop'
     * needed
     * @see icera.DebugAgent.LogSaver#start()
     */
    @Override
    public boolean start() {
        if (!beforeStart())
            return false;
  //      addToLogList(mLog);
        updateLogList();
        calculateLogSize();

        return true;
    }

    private void init() {
        mFileUtiles = new FileUtiles();
        mPowerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mWakeLock = mPowerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, TAG);
    }

    private void acquireWakeLock() {
        if (mWakeLock != null) {
            Log.v(TAG, "Acquiring wake lock the wake lock !!!");
            mWakeLock.acquire();
        } else {
            Log.d(TAG, "WakeLock object not initialised ");
        }
    }

    private void releaseWakeLock() {
        if (mWakeLock != null) {
            if (mWakeLock.isHeld()){
                mWakeLock.release();
                Log.v(TAG, "Releasing the wake lock !!!");
            }else{
                Log.d(TAG,"Wake log not held");
            }
        } else {
            Log.d(TAG, "WakeLock object not initialised ");
        }
    }

    private String getIMEI() {
        return ((TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE))
               .getDeviceId();
    }

    private String getPhoneNumber() {
        TelephonyManager tm = (TelephonyManager)
        mContext.getSystemService(Context.TELEPHONY_SERVICE);
        return tm.getLine1Number();
    }

    synchronized void collectCompressLogs() {
        Log.i(TAG, "collectCompressLogs start");
        getDeviceInfo();
        dumpLogs();
        mFileUtiles.copyFile(TRACE_TXT, mDumpTrace);
        mFileUtiles.clearFile(TRACE_TXT);
        mFileUtiles.copyFile(PANIC_TXT, mDumpPanic);
        mFileUtiles.clearFile(PANIC_TXT);
        mFileUtiles.zipFiles(mLoggingDir.getAbsolutePath(), mLoggingDir.getParent() + "/nvlogdump.zip");
        mFileUtiles.createGzip(mZipFile, mGZipFile);
        Log.i(TAG, "collectCompressLogs done");
    }

    /**
     * dumpLogs(), function collects logs such as usagestats,logcat, meminfo and
     * dmesg running in a thread and dumps this logs into dump directory.
     */
    private void dumpLogs() {
        long startTime = SystemClock.elapsedRealtime();
        try {
            Commands mCommands = new Commands();
            mCommands.runCommand("logcat", "logcat -v time -d -f " + mDumpLogcat, 50, null);
            mCommands.runCommand("usage", "dumpsys usagestats", 30, mDumpUsageStat);
            mCommands.runCommand("meminfo", "dumpsys meminfo", 30, mDumpMemInfo);
            mCommands.runCommand("dmseg", "dmesg", 30, mDumpDmesg);
            mCommands = null;
        } catch (IOException e) {
            Log.e(TAG, "Error while running command");
            e.printStackTrace();
        }
        long endTime = SystemClock.elapsedRealtime();
        Log.i(TAG, "Time for Dumping logs :" + (endTime - startTime) + "msec");
    }

    public void SaveLogsToUserDir(){
        mFileUtiles.createGzip(mZipFile, mGZipFile);
        File logs = new File(mGZipFile);
        String path = Environment.getExternalStorageDirectory().getPath() + "/BugReport_Logs/"
        + new SimpleDateFormat("MMM-dd_HH-mm-ss",Locale.US).format(new Date())
        + "_" + "AP_LOGS";
        (new File(path)).mkdir();
        logs.renameTo(new File(path + File.separator + "nvlogdump.zip.gz"));
    }

    /**
     * getDeviceInfo() function collects device info, such such device id,
     * board, model etc... and writes it to a file.
     *
     */
    private void getDeviceInfo() {
        StringBuilder deviceInfo = new StringBuilder();
        deviceInfo.append("Bug Title:" + "Applacation Crashes." + "\n");
        deviceInfo.append("Reproduce steps:" + "\n");
        deviceInfo.append("Mobile IMEI:" + getIMEI() + "\n");
        deviceInfo.append("Mobile Number:" + getPhoneNumber() + "\n");
        deviceInfo.append("-----------build.prop-------------\n");
        deviceInfo.append("ANDROID_ID:"+ Secure.getString(mContext.getContentResolver(), Secure.ANDROID_ID)+ "\n");
        deviceInfo.append("Build.BOARD:" + android.os.Build.BOARD + "\n");
        deviceInfo.append("Build.BRAND:" + android.os.Build.BRAND + "\n");
        deviceInfo.append("Build.CPU_ABI:" + android.os.Build.CPU_ABI + "\n");
        deviceInfo.append("Build.DEVICE:" + android.os.Build.DEVICE + "\n");
        deviceInfo.append("Build.DISPLAY:" + android.os.Build.DISPLAY + "\n");
        deviceInfo.append("Build.FINGERPRINT:" + android.os.Build.FINGERPRINT + "\n");
        deviceInfo.append("Build.HARDWARE:" + android.os.Build.HARDWARE + "\n");
        deviceInfo.append("Build.HOST:" + android.os.Build.HOST + "\n");
        deviceInfo.append("Build.ID:" + android.os.Build.ID + "\n");
        deviceInfo.append("Build.MANUFACTURER:" + android.os.Build.MANUFACTURER    + "\n");
        deviceInfo.append("Build.MODEL:" + android.os.Build.MODEL + "\n");
        deviceInfo.append("Build.PRODUCT:" + android.os.Build.PRODUCT + "\n");
        deviceInfo.append("Build.TAGS:" + android.os.Build.TAGS + "\n");
        deviceInfo.append("Build.VERSION.CODENAME:"    + android.os.Build.VERSION.CODENAME + "\n");
        deviceInfo.append("Build.VERSION.INCREMENTAL:" + android.os.Build.VERSION.INCREMENTAL + "\n");
        deviceInfo.append("Build.VERSION.RELEASE:" + android.os.Build.VERSION.RELEASE + "\n");
        deviceInfo.append("Build.VERSION.VERSION.SDK:" + android.os.Build.VERSION.SDK + "\n");
        try {
            FileOutputStream out = new FileOutputStream(mDeviceInfo);
            BufferedOutputStream bos = new BufferedOutputStream(out);
            bos.write(deviceInfo.toString().getBytes());
            bos.close();
            out.close();
        } catch (FileNotFoundException e) {
            Log.e(TAG,"Error FileNotFoundException in function getDeviceInfo()");
            e.printStackTrace();
        } catch (IOException e) {
            Log.e(TAG, "Error IOException in function getDeviceInfo()");
            e.printStackTrace();
        }
        deviceInfo = null;
    }

    public void bugreport() {
        acquireWakeLock();

        Thread WorkerThread = new Thread(new Runnable() {
            @Override
            public void run() {
                collectCompressLogs();
                releaseWakeLock();
            }
        });
        WorkerThread.start();
        try {
            WorkerThread.join();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void deleteLogs() {
        mFileUtiles.delete(mZipFile);
        mFileUtiles.delete(mLoggingDir.getAbsolutePath());
        super.deleteLogs();
    }
}
