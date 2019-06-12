// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import android.content.Context;
import android.util.Log;

import icera.DebugAgent.LogUtil.StoreProgressListener;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

public class BugreportDumper extends LogDumper {

    private static final String TAG = "IDA.Bugreport";

    private static final String BUGREPORT_FILE_NAME = "BugReport.txt";

    public BugreportDumper(Context ctx) {
        super(ctx);
        // TODO Auto-generated constructor stub
    }

    @Override
    protected void setLogFileName() {
        mLogFileName = BUGREPORT_FILE_NAME;
    }

    @Override
    protected void prepareProgram() {

        mProgram.clear();
        mProgram.add("/system/bin/dumpstate");

    }

    @Override
    public boolean start() {
        // Don't start if precondition not met
        if (!beforeStart())
            return false;

        // Prepare the program and arguments to run
        prepareProgram();

        if (mProgram.isEmpty())
            return false;

        addToLogList(mLog);
        mTotalLogSize = 4 * 1024 * 1024; // pseudo size

        return true;
    }

    @Override
    protected void updateLogList() {

    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "BUGREPORT";
    }

    Runnable mBugreportRunnable = new Runnable() {
        public void run() {

            Process proc = null;
            try {
                // Runtime.getRuntime().exec("/system/xbin/su root /system/bin/cat /proc/kmsg");
                proc = new ProcessBuilder().command(mProgram.toArray(new String[0]))
                        .redirectErrorStream(true).start();
                // proc = Runtime.getRuntime().exec(mProgram.toArray(new
                // String[0]));
            } catch (IOException e) {
                Log.e(TAG, "Failed to run bugreport");
                e.printStackTrace();
                return;
            }

            BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream()),
                    1024);

            FileOutputStream fos = null;
            try {
                fos = new FileOutputStream(mLog);
                String line;
                while ((line = br.readLine()) != null) {
                    line += "\n";
                    // Log.d(TAG, line);
                    fos.write(line.getBytes());
                }
            } catch (FileNotFoundException e) {
                Log.e(TAG, "Failed to open log file for write: " + mLog.getAbsolutePath());
                e.printStackTrace();
            } catch (IOException e) {
                Log.e(TAG, "Error when reading the logging output or writing the log");
                e.printStackTrace();
            } finally {
                if (proc != null) {
                    proc.destroy();
                    proc = null;
                }
                if (br != null) {
                    try {
                        br.close();
                    } catch (IOException e1) {
                        e1.printStackTrace();
                    }
                }
                if (fos != null) {
                    try {
                        fos.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }

            updateLogList();
            calculateLogSize();
        }
    };

    /*
     * Call 'bugreport' here
     */
    @Override
    public void store(File destDir, boolean onSD, StoreProgressListener spl) {

        Thread dumpThread = new Thread(mBugreportRunnable);
        if (dumpThread != null) {
            try {
                dumpThread.start();
                dumpThread.join(600000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        super.store(destDir, onSD, spl);
    }

}
