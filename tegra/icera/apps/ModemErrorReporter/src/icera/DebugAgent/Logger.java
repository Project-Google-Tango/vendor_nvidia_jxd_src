// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.File;
import java.io.IOException;

import android.content.Context;
import android.util.Log;

public abstract class Logger extends LogUtil {

    static final String TAG = "Logger";

    protected Process mLoggingProc = null;

    protected boolean mLogging = false;

    public Logger(Context ctx) {

        super(ctx);

    }

    protected abstract void prepareProgram();

    public boolean start() {

        // Don't start if precondition not met
        if (!beforeStart())
            return false;

        // Prepare the program and arguments to run
        prepareProgram();

        if (mProgram.isEmpty())
            return false;

        try {
            mLoggingProc = Runtime.getRuntime().exec(mProgram.toArray(new String[0]));
            mLogging = true;
            for (String str: mProgram.toArray(new String[0])){
                Log.d(TAG, "Logger command param:" + str);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to run logger program");
            mLoggingProc = null;
            mLogging = false;

            e.printStackTrace();
        }

        return mLogging;
    }

    public void stop() {

        killLogProc();
        updateLogList();
        calculateLogSize();
    }

    public void stopAndClean() {

        stop();
        deleteLogs();
    }

    protected void killLogProc() {
        if (mLoggingProc != null)
            mLoggingProc.destroy();

        mLoggingProc = null;
        mLogging = false;
    }

    public final boolean isLogging() {

        return mLogging;
    }

    @Override
    public void store(File destDir, boolean onSD, StoreProgressListener spl) {

        if (mLogging)
            stop();

        super.store(destDir, onSD, spl);
    }
}
