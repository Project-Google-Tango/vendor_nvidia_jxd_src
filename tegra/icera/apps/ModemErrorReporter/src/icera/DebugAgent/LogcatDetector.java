// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.LinkedList;
import java.util.Queue;

import android.content.Context;
import android.util.Log;

/**
 * @author jeffreyz
 */
public class LogcatDetector extends LogDumper {

    private static final String TAG = "LogcatDetector";

    private boolean mRilDetected = false;

    private String mStrToDetectRil;
    private String mStrToDetectMdm;

    /**
     * @param ctx
     */
    public LogcatDetector(Context ctx, String strToDetect) {

        super(ctx);

        onSDCard = false; // use default logging folder on flash
        mStrToDetectRil = strToDetect;
    }

    /**
     * @param ctx
     */
    public LogcatDetector(Context ctx, String strToDetectRil, String strToDetectMdm) {

        super(ctx);

        onSDCard = false; // use default logging folder on flash
        mStrToDetectRil = strToDetectRil;
        mStrToDetectMdm = strToDetectMdm;
    }

    @Override
    protected void setLogFileName() {

        mLogFileName = "LogcatDetector.txt";
    }

    @Override
    protected void setLoggerName() {

        mLoggerName = "LCD";
    }

    @Override
    protected void prepareProgram() {

        mProgram.clear();
        mProgram.add("logcat");
        mProgram.add("-d");
        mProgram.add("-v");
        mProgram.add("threadtime");
        mProgram.add("-b");
        mProgram.add("radio");

    }

    @Override
    protected void updateLogList() {

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

        Process proc = null;

        try {
            proc = Runtime.getRuntime().exec(mProgram.toArray(new String[0]));
        } catch (IOException e) {

            Log.e(TAG, "Failed to run the logcat detector program");
            e.printStackTrace();
            return false;
        }

        BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream()));
        try {
            String line, lastLine = "";
            //Save the buffer's content, check the kernel time when RIL_INIT keyword comes out.
            Queue<String> que = new LinkedList<String>();
            //Count the lines of ril log. Assumes the line number doesn't exceed a integer
            int i = 0;
            while ((line = br.readLine()) != null) {
                if (line.contains(mStrToDetectMdm))
                {
                    mRilDetected = false;
                    break;
                }

                if (!line.contains(mStrToDetectRil)) {
                    que.offer(line);
                    if (i++ > 10){
                        que.poll();
                    }
                    continue;
                }
                else {
                    /*
                     * Parse the kernel time in the last message. If the value is small,
                     * that's to say ril normally start when system power on.
                     */
                    String sub = null;
                    while ((lastLine = que.poll()) != null){
                         sub = getSubString(lastLine, "Kernel time: [", ".");
                         if (sub != null){
                             Log.d(TAG, "get kernel time info:" + lastLine);
                             break;
                         }
                    }
                    if (sub == null) {
                        mRilDetected = true;
                        break;
                    }
                    Log.d(TAG, "get kernel integer time:" + sub);
                    int kernelTime = Integer.parseInt(sub);
                    if (kernelTime > 30) {
                        mRilDetected = true;
                    }
            break;
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Error when reading the detector output");
            e.printStackTrace();
            return false;
        } finally {
            if (proc != null)
                proc.destroy();

            try {
                br.close();
            } catch (IOException e1) {
                e1.printStackTrace();
            }
        }

        return true;
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

    public boolean isDetected() {
        return mRilDetected;
    }
}
