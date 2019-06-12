// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import android.content.Context;
import java.io.File;

public class LogcatSystem extends Logcat {

    private static final String LOG_FILE_NAME = "logcat_system.txt";

    public LogcatSystem(Context ctx) {
        super(ctx);

    }

    /*
     * @see icera.DebugAgent.Logcat#prepareProgram()
     */
    @Override
    protected void prepareProgram() {
        mProgram.clear();
        mProgram.add("logcat");
        mProgram.add("-v");
        mProgram.add("threadtime");
        mProgram.add("-b");
        mProgram.add("system");
        mProgram.add("-f");
        mProgram.add(mLoggingDir.getAbsolutePath() + File.separator + LOG_FILE_NAME);
    }

    /*
     * (non-Javadoc)
     * @see icera.DebugAgent.Logcat#setLoggerName()
     */
    @Override
    protected void setLoggerName() {
        mLoggerName = "LCSYSTEM";
    }

    @Override
    protected void updateLogList() {

        addToLogList(new File(mLoggingDir.getAbsolutePath() + File.separator + LOG_FILE_NAME));
    }
}
