// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import android.os.Handler;
import android.os.Message;
import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

/**
 * This monitors RADIO log for predefined scenarios, and report the event to the service.
 * The handler of main thread is needed.
 * 
 * @author jeffreyz
 *
 */
public class AutoSaveMonitor extends Thread {

    private static final String TAG = "IDA.AutoSaveMonitor";

    public static final int CANCEL_SCENARIO = -1;

    AutoSaveScenario[] mScenarios;
    AutoSaveScenario mDetectedScenario = null;
    Handler mSvcHandler = null;
    private int mCFUNstate = 1;
    String[] mCFUN0Msg, mCFUN1Msg;

    private boolean mStop = false;
    private boolean mPauseParsing = false;

    /**
     * 
     */
    public AutoSaveMonitor(Handler svcHandler, AutoSaveScenario[] scenarios, String[] CFUN0Msg, String[] CFUN1Msg) {

        mScenarios = scenarios;
        mSvcHandler = svcHandler;
        mCFUN0Msg = CFUN0Msg;
        mCFUN1Msg = CFUN1Msg;
    }

    /* 
     * 
     */
    @Override
    public void run() {
        List<String> program = new ArrayList<String>();
        program.clear();
        program.add("logcat");
        program.add("-v");
        program.add("threadtime");
        program.add("-b");
        program.add("radio");

        Process proc = null;

        try {
            proc = Runtime.getRuntime().exec(program.toArray(new String[0]));
        } catch (IOException e) {

            Log.e(TAG, "Failed to run the logcat detector program");
            e.printStackTrace();
            return;
        }

        BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream()));
        try {

            String line;
            while (!mStop  && ((line = br.readLine()) != null)) {
                
                if (mCFUNstate == 1) {
                
                    for (String msg : mCFUN0Msg)
                        if (line.contains(msg)) {
                            mCFUNstate = 0;
                            Log.i(TAG, "CFUN: 1 -> 0");
                            break;
                        }
                    if (mCFUNstate == 0 ) continue;

                    if (!mPauseParsing) {
                        for (AutoSaveScenario scenario : mScenarios) {
                            if (scenario.occurrs(line)) {
                                Message msg = Message.obtain(mSvcHandler, scenario.id);
                                mSvcHandler.sendMessage(msg);
                                Log.i(TAG, "Detected a scenario: " + scenario.dirSuffix);
                                mPauseParsing = true;
                                mDetectedScenario = scenario;
                                break;
                            }
                        }
                    }
                    else if ((mDetectedScenario != null) && mDetectedScenario.isCancelable()) {
                        if (mDetectedScenario.cancelingOccurrs(line)) {
                            Message msg = Message.obtain(mSvcHandler, CANCEL_SCENARIO);
                            mSvcHandler.sendMessage(msg);
                            Log.i(TAG, "Detected scenario canceling keyword.");
                        }
                    }
                } else if (mCFUNstate == 0) {
                    
                    for (String msg : mCFUN1Msg) {
                        if (line.contains(msg)) {
                            mCFUNstate = 1;
                            Log.i(TAG, "CFUN: 0 -> 1");
                            break;
                        }
                    }
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Error when reading the detector output");
            e.printStackTrace();
            return;
        } finally {
            if (proc != null)
                proc.destroy();

            try {
                br.close();
            } catch (IOException e1) {
                e1.printStackTrace();
            }
        }

    }
    public void stopParsing(){
        mPauseParsing = true;
        mDetectedScenario = null;
    }
    public void resumeParsing() {
        this.mPauseParsing = false;
        mDetectedScenario = null;
    }
}
