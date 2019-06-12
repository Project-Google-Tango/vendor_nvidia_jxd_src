// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import icera.DebugAgent.R;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.util.Log;

public class Cleanup extends Activity {
    /** Called when the activity is first created. */

    public void onCreate(Bundle savedInstanceState) {

        Log.d("Icera", "Creating view");

        super.onCreate(savedInstanceState);

        // Display main screen
        setContentView(R.layout.cleanup);

        Log.d("Icera", "View created");
    }

    @Override
    public void onBackPressed() {
        Cleanup.this.finish();
    }

    public void DisplayLogs(View view) {
        // Action to be executed on click.
        Intent DisplayLogs = new Intent(this, DisplayLogs.class);
        startActivity(DisplayLogs);
    }

    public void Settings(View view) {
        // Action to be executed on click.
        Intent settings = new Intent(this, SaveLogs.class);
        settings.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(settings);
    }
}
