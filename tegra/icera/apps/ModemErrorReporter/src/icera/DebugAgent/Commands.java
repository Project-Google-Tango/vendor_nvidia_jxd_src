// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import android.os.SystemClock;
import android.util.Log;

public class Commands {

    /**
     * runCommand: This function runs the command to be executed.
     *
     * @param commandTag
     *            Tag or identifier for given command
     * @param command
     *            is actual command to be execute.
     * @param timeout
     *            kill the process once timeout value is reached
     * @return Exit code of given command.
     * @throws IOException
     */

    public int runCommand(String commandTag, String command, int timeout,
            String outFile) throws IOException {
        String TAG = "runCommand";
        Log.d(TAG, "onStart function runCommand()");
        int exitStatus = 2;
        int bailout = timeout;
        BufferedReader inBuffer = null;
        BufferedWriter out = null;
        Process process = null;
        String tempOutbuffer = null;
        Log.i(TAG, commandTag + ":");
        process = Runtime.getRuntime().exec((command));
        inBuffer = new BufferedReader(new InputStreamReader(
                process.getInputStream()));

        while (timeout != 0)
            try {
                exitStatus = process.exitValue();
                Log.i(TAG, "Exit status :" + exitStatus);
                break;
            } catch (IllegalThreadStateException e) {
                SystemClock.sleep(500);
                //Log.d(TAG, "Sleeping for 500 msec");
                timeout -= 1;
            }

        if (outFile != null) {
            out = new BufferedWriter(new FileWriter(outFile));
            Log.d(TAG, "Writing to file " + outFile);
            while ((tempOutbuffer = inBuffer.readLine()) != null) {
                out.write(tempOutbuffer + "\n");
            }
            out.close();
        } else {
            while ((tempOutbuffer = inBuffer.readLine()) != null) {
                Log.d(TAG, tempOutbuffer + "\n");
            }
        }
        inBuffer.close();
        process.destroy();

        if (timeout == 0)
            Log.e(TAG, "Bailout Reached " + command + " Didn't exit within:"+ bailout + " sec");
        Log.d(TAG, "onExit function runCommand()");
        return exitStatus;
    }
}