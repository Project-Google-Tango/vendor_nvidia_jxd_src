package modem.SerialTest;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.PendingIntent;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.telephony.TelephonyManager;

import android.os.Bundle;
import android.os.SystemClock;
import android.os.Vibrator;

import android.widget.TextView;
import android.widget.Toast;

import android.util.Log;

public class StartApplication extends Activity {

    private static final String TAG = "SerialTest.StartApplication";

    static final String ACTION_ALARM = "serialtest.action.alarm";

    /** counter to keep track of success rate */
    int passCount = 0;
    int testCount = 0;

    /** Telephony manager */
    TelephonyManager tm;

    /** UI items */
    Toast mToast;
    TextView tv;
    

    public BroadcastReceiver alarmReceiver = new BroadcastReceiver() {
	    @Override
	    public void onReceive(Context context, Intent intent) {
		Log.v("AlarmReceiver","Alarm received");
		runOnUiThread( new Runnable() {
			    public void run() {
				alarmHandler();
			    }
			} );
	    };
	};

    /** Called by alarmReceiver */
    public void alarmHandler()
    {
	String operatorName = tm.getNetworkOperatorName();

	Log.v(TAG, "Operator name = '" + operatorName + "'");

	if ( (operatorName!=null) && (!operatorName.contentEquals("")) ) {
	    passCount++;
	}
	testCount++;

	tv.setText("Success rate = " + passCount + "/" + testCount);
    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        int periodMs;
        String periodArg;

        tv = new TextView(this);
        tv.setText("Serial Test");
        setContentView(tv);

	// get telephony manager
	tm = (TelephonyManager)getSystemService(TELEPHONY_SERVICE);

        // When the alarm goes off, we want to broadcast an Intent to our
        // BroadcastReceiver.  Here we make an Intent with an explicit class
        // name to have our own receiver (which has been published in
        // AndroidManifest.xml) instantiated and called, and then create an
        // IntentSender to have the intent executed as a broadcast.
        // Note that unlike above, this IntentSender is configured to
        // allow itself to be sent multiple times.
        Intent intent = new Intent(StartApplication.this, RepeatingAlarm.class);
        PendingIntent sender = PendingIntent.getBroadcast(StartApplication.this,
                                                          0, intent, 0);

        // get wake period from extra parameters
        try {
            periodArg = getIntent().getStringExtra("period");
            periodMs = Integer.decode(periodArg).intValue();
            Log.v(TAG,"Setting period to " + periodMs + " ms");
        } catch (Exception e) {
            Log.v(TAG,"Setting period to default value (30s)");
            periodMs = 30000;
        }

        // We want the alarm to go off 30 seconds from now.
        long firstTime = SystemClock.elapsedRealtime();
        firstTime += periodMs;

        // Schedule the alarm!
        AlarmManager am = (AlarmManager)getSystemService(ALARM_SERVICE);
        am.setRepeating(AlarmManager.ELAPSED_REALTIME_WAKEUP,
                        firstTime, periodMs, sender);

        // Tell the user about what we did.
        if (mToast != null) {
            mToast.cancel();
        }

	// Light message
        mToast = Toast.makeText(StartApplication.this, "Alarm scheduled",
                                Toast.LENGTH_LONG);
        mToast.show();
        Log.v(TAG, "Alarm scheduled every " + periodMs + " ms");

	// Register receiver
	registerReceiver(alarmReceiver, new IntentFilter(ACTION_ALARM));

        Vibrator vibrator = new Vibrator();
        vibrator.vibrate(200);
    }
}
