// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

/**
 * 
 */

package icera.DebugAgent;

import android.content.Context;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * @author jeffreyz
 */
public class KernelLogger extends Logger {

    private static final String TAG = "IDA.KLogger";

    private static final String KERNEL_LOG_FILE_NAME = "kmsg";

    private String mLogFileName;

    private File mLog;

    private boolean mRunning;

    private ExecutorService EX;

    /**
     * @param ctx
     */
    public KernelLogger(Context ctx) {
        super(ctx);
        // TODO Auto-generated constructor stub
    }

    /*
     * 
     */
    protected void setLogFileName() {
        mLogFileName = KERNEL_LOG_FILE_NAME;

    }

    /*
     * (non-Javadoc)
     * @see icera.DebugAgent.LogUtil#prepareProgram()
     */
    @Override
    protected void prepareProgram() {
        mProgram.clear();
        // mProgram.add("/system/xbin/su");
        // mProgram.add("root");
        mProgram.add("/system/bin/dumpstate");
        // mProgram.add("-c");
        // mProgram.add("su");
        // mProgram.add("cat");
        // mProgram.add("/proc/kmsg");
    }

    protected final boolean createLogFile() {

        setLogFileName();

        if (mLogFileName == null) {
            Log.e(TAG, "Log file name is not specified");
            return false;
        }

        mLog = new File(mLoggingDir.getAbsolutePath() + File.separator + mLogFileName);
        try {
            if (!mLog.createNewFile()) {
                if (!mLog.isFile())
                    return false;
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to create log file");
            e.printStackTrace();
            return false;
        }

        return true;
    }

    /*
     * (non-Javadoc)
     * @see icera.DebugAgent.LogUtil#beforeStart()
     */
    @Override
    protected boolean beforeStart() {
        // call super to set up logging directory
        if (!super.beforeStart())
            return false;

        return createLogFile();
    }

    /*
     * (non-Javadoc)
     * @see icera.DebugAgent.Logger#start()
     */
    @Override
    public boolean start() {

        // Don't start if precondition not met
        if (!beforeStart())
            return false;

        // Prepare the program and arguments to run
        prepareProgram();

        if (mProgram.isEmpty())
            return false;

        EX = Executors.newSingleThreadExecutor();
        EX.execute(new Runnable() {
            public void run() {

                Process proc = null;
                try {
                    // Runtime.getRuntime().exec("/system/xbin/su root /system/bin/cat /proc/kmsg");
                    proc = new ProcessBuilder().command(mProgram.toArray(new String[0]))
                            .redirectErrorStream(true).start();
                    // proc = Runtime.getRuntime().exec(mProgram.toArray(new
                    // String[0]));
                } catch (IOException e) {
                    Log.e(TAG, "Failed to run the logging program");
                    e.printStackTrace();
                    return;
                }

                BufferedReader br = new BufferedReader(
                        new InputStreamReader(proc.getInputStream()), 512);
                // FileInputStream fis = null;

                FileOutputStream fos = null;
                try {
                    fos = new FileOutputStream(mLog);
                    // fis = new FileInputStream(new File("/proc/kmsg"));
                    // BufferedInputStream bis = new BufferedInputStream(fis,
                    // 1024);

                    mRunning = true;
                    String line;
                    while (mRunning /* && (line = br.readLine()) != null */) {
                        if (!mRunning) {
                            break;
                        }
                        line = br.readLine();
                        if (line == null || line.length() == 0) {
                            Thread.sleep(1000);
                            try {
                                Log.d(TAG, "exitValue: " + proc.exitValue());
                            } catch (IllegalThreadStateException e) {
                                continue;
                            }
                            continue;
                        }
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
                } catch (InterruptedException e) {
                    // TODO Auto-generated catch block
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
            }
        });

        return true;
    }

    @Override
    public void stop() {
        mRunning = false;

        if (EX != null && !EX.isShutdown()) {
            EX.shutdown();
            EX = null;
        }

        updateLogList();
        calculateLogSize();
    }

    /*
     * (non-Javadoc)
     * @see icera.DebugAgent.LogUtil#setLoggerName()
     */
    @Override
    protected void setLoggerName() {
        mLoggerName = "KLOG";
    }

    /*
     * (non-Javadoc)
     * @see icera.DebugAgent.LogUtil#updateLogList()
     */
    @Override
    protected void updateLogList() {
        this.addToLogList(mLog);
    }
}
