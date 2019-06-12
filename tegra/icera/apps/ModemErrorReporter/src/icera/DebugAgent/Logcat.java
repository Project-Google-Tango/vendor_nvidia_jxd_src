// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import android.content.Context;
import android.util.Log;
import android.widget.Toast;

import java.io.*;

public class Logcat extends Logger {

    private static final String TAG = "Logcat";

    public static final String CRASH_INFO_FILE = "coredump.txt";

    private static final String LOG_FILE_NAME = "logcat_ril_main_system_events.txt";

    public Logcat(Context ctx) {

        super(ctx);

        createCrashInfoFile();

    }

    private void createCrashInfoFile() {

        File single = new File("/data/rfs/data/debug");
        if (!(single.exists())) {
            // re-create the coredump.txt which is used by the RIL
            try {
                String string = "No Crash Happened";
                FileOutputStream file_coredump = mContext.openFileOutput(CRASH_INFO_FILE,
                        Context.MODE_WORLD_READABLE | Context.MODE_WORLD_WRITEABLE);
                file_coredump.write(string.getBytes());
                file_coredump.close();
                Toast.makeText(mContext, "core info file created", Toast.LENGTH_LONG).show();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }

    }

    @Override
    public boolean start() {

        if (super.start()) {
            Log.d(TAG, "Logcat started");
            return true;
        } else {
            Log.e(TAG, "Failed to start logcat");
            return false;
        }
    }

    // Stop logcat logging
    @Override
    public void stopAndClean() {

        Log.d(TAG, "Logcat stopped");

        super.stopAndClean();
    }

    public String getCrashInfo() {

        StringBuilder sb = new StringBuilder();
        String line = "";
        Boolean getCrashLog = false;

        try {

            Process p = Runtime.getRuntime().exec(new String[] {
                    "logcat", "-d", "-b", "radio"
            });
            BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));

            while ((line = br.readLine()) != null) {

                if (line.contains("DXP0 Crash Report"))
                    getCrashLog = true;
                else if (line.contains("Register"))
                    getCrashLog = false;
                else if (line.contains("Fullcoredump"))
                    getCrashLog = false;
                else if (line.contains("DXP1 Crash Report")) {
                    sb.append('\n');
                    sb.append('\n');
                    getCrashLog = true;
                }

                if (getCrashLog) {
                    String[] temp = line.split("<");
                    sb.append(temp[1]);
                    sb.append('\n');
                    // Restart the logging after a crash
                    // if (mPrefs.isModemLoggingActive())
                    // ((BugReportService) mContext).startMdmLogging();
                }
            }

        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        return sb.toString();
    }

    public boolean getCrash() {

        boolean RIL_Crash = false;
        Process p = null;
        try {

            p = Runtime.getRuntime().exec(new String[] {
                    "logcat", "-d", "-v", "threadtime", "-b", "radio"
            });
        } catch (IOException e) {

            Log.e(TAG, "Failed to run the logcat detector program");
            e.printStackTrace();
            return false;
        }

        BufferedReader br = new BufferedReader(new InputStreamReader(p.getInputStream()));
        try {
            String line, lastLine = "";
            while ((line = br.readLine()) != null) {

                if (!line.contains("RIL_Init"))
                {
                    lastLine = line;
                    continue;
                }
                else {
                    Log.d(TAG, "get kernel time info:" + lastLine);
                    /*
                     * Parse the kernel time in the last message. If the value is small,
                     * that's to say ril normally start when system power on.
                     */
                    String sub = getSubString(lastLine, "Kernel time: [", ".");
                    if (sub == null) {
                        RIL_Crash = true;
                        break;
                    }
                    Log.d(TAG, "get kernel integer time:" + sub);
                    int kernelTime = Integer.parseInt(sub);
                    if (kernelTime > 30) {
                        RIL_Crash = true;
                    }
                    break;
                }
            }
            while ((line = br.readLine()) != null) {

                if (line.contains("DXP0 Crash Report"))
                {
                    RIL_Crash = false;
                    break;
                }
            }

        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return false;
        } finally {
            if (p != null)
                p.destroy();

            try {
                br.close();
            } catch (IOException e1) {
            e1.printStackTrace();
            }
        }
        return RIL_Crash;
    }

    String getSubString(String line, String start, String end) {
        if (line == null || start == null || end == null)
            return null;
        int index = line.indexOf(start);
        if (index < 0) {
            return null;
        }
        index += start.length();
        //line.substring(index,  index + line.substring(index).indexOf("."));
        return line.substring(index,  index + line.substring(index).indexOf(end));
    }


    @Override
    protected void prepareProgram() {

        mProgram.add("logcat");
        mProgram.add("-v");
        mProgram.add("threadtime");
        mProgram.add("-b");
        mProgram.add("main");
        mProgram.add("-b");
        mProgram.add("radio");
        mProgram.add("-b");
        mProgram.add("system");
        mProgram.add("-b");
        mProgram.add("events");
        mProgram.add("-f");
        mProgram.add(mLoggingDir.getAbsolutePath() + File.separator + LOG_FILE_NAME);
    }

    @Override
    protected void updateLogList() {

        addToLogList(new File(mLoggingDir.getAbsolutePath() + File.separator + LOG_FILE_NAME));
    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "LOGCAT";
    }
}
