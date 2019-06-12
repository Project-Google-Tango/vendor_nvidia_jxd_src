// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import android.content.Context;
import android.util.Log;

/**
 * This is for the one-shot run then stop dumper logger, like coredump and
 * kernel the log files are collected in 'start' right after
 * 
 * @author jeffreyz
 */
public abstract class LogDumper extends LogUtil {

    private static final String TAG = "LogDumper";

    protected String mLogFileName = null;

    protected File mLog = null;

    /**
     * @param ctx
     */
    public LogDumper(Context ctx) {

        super(ctx);
    }

    protected abstract void setLogFileName();

    protected final boolean createLogFile() {

        setLogFileName();

        if (mLogFileName == null) {
            Log.e(TAG, "Log file name is not specified");
            return false;
        }

        mLog = new File(mLoggingDir.getAbsolutePath() + File.separator + mLogFileName);

        return true;
    }

    @Override
    protected boolean beforeStart() {

        // call super to set up logging directory
        if (!super.beforeStart())
            return false;

        return createLogFile();
    }

    /*
     * Run the logging program and collect the log files, in one-shot. no 'stop'
     * needed
     * @see icera.DebugAgent.LogSaver#start()
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

        Process proc = null;

        try {
            proc = new ProcessBuilder().command(mProgram.toArray(new String[0]))
                    .redirectErrorStream(true).start(); // Runtime.getRuntime().exec(mProgram.toArray(new
                                                        // String[0]));
        } catch (IOException e) {
            Log.e(TAG, "Failed to run the logging program");
            e.printStackTrace();
            return false;
        }

        Log.d(TAG, "LogDumper started.");

        StringBuilder sb = new StringBuilder();
        BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream()));
        try {
            String line;
            while ((line = br.readLine()) != null) {
                sb.append(line);
                sb.append('\n');
            }
            // proc.destroy();
        } catch (IOException e) {
            Log.e(TAG, "Error when reading the logging output");
            e.printStackTrace();
            return false;
        } finally {
            // proc.destroy();
            try {
                br.close();
            } catch (IOException e1) {
                // TODO Auto-generated catch block
                e1.printStackTrace();
            }
        }

        FileOutputStream logfile = null;
        try {
            logfile = new FileOutputStream(mLog);
            logfile.write(sb.toString().getBytes());
            logfile.close();
        } catch (FileNotFoundException e) {
            Log.e(TAG, "Failed to open log file for write: " + mLog.getAbsolutePath());
            e.printStackTrace();
            return false;
        } catch (IOException e) {
            Log.e(TAG, "Error when writing to log file: " + mLog.getAbsolutePath());
            e.printStackTrace();
            try {
                logfile.close();
            } catch (IOException e1) {
                e1.printStackTrace();
            }
            return false;
        }

        addToLogList(mLog);
        updateLogList();
        calculateLogSize();

        return true;
    }
}
