// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import icera.DebugAgent.BugReportService;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

public class IceraDebugAgentActivity extends Activity {

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        Prefs prefs = new Prefs(this);

        parseCommandLine(prefs);

        // Start the background service.
        Intent startupIntent = new Intent(IceraDebugAgentActivity.this, BugReportService.class);
        startService(startupIntent);

        // Nothing to display so close the activity.
        this.finish();
    }

    /*
     * Allow activity to be lauched at command line 
     * $adb root 
     * $adb shell am start -n icera.DebugAgent/.IceraDebugAgentActivity --ez cmdLineParam true --ez "launch" true --ez "logging" true --ez "autoSave" true --ez "logcat" true --ez
     * "SDcard" true --activity-clear-top --activity-clear-task
     */
    private void parseCommandLine(Prefs prefs) {

        Intent intent = getIntent();

        if (intent.getBooleanExtra("cmdLineParam", false)) {
            prefs.setAutoLaunchEnabled(intent.getBooleanExtra("launch", true));
            prefs.setModemLoggingEnabled(intent.getBooleanExtra("logging", true));
            prefs.setLogcatEnabled(intent.getBooleanExtra("logcat", true));
            prefs.setAutoSavingEnabled(intent.getBooleanExtra("autoSave", true));
            prefs.setLoggingOnSDCard(intent.getBooleanExtra("SDcard", true));
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        // The activity is about to become visible.
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

}
