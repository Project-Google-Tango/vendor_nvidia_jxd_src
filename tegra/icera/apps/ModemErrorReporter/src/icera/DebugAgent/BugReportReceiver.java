// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import icera.DebugAgent.BugReportService.NotificationType;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class BugReportReceiver extends BroadcastReceiver {

    /**
     * Handle check of logcat depending on the phone state.
     */

    @Override
    public void onReceive(Context context, Intent intent) {
        Intent startupIntent = new Intent(context, BugReportService.class);

        if (intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            boolean autoLaunch = new Prefs(context).isAutoLaunchEnabled();
            if (autoLaunch) {

                // startupIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startService(startupIntent);
            }
        }

        if (intent.getAction().equals("com.nvidia.feedback.NVIDIAFEEDBACK")) {
            Intent crashdetected = new Intent();
            crashdetected.setAction("ModemCrash");
            crashdetected.putExtra("tag", "modem_crash");
            context.sendBroadcast(crashdetected);
        }
    }
}