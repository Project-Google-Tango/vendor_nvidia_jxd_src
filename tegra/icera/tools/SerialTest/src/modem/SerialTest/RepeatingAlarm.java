/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package modem.SerialTest;

import android.content.Context;
import android.content.Intent;
import android.content.BroadcastReceiver;

import android.os.PowerManager;

import android.util.Log;

import android.app.Activity;

import java.lang.Thread;

/**
 * This is an example of implement an {@link BroadcastReceiver} for an alarm that
 * should occur once.
 */
public class RepeatingAlarm extends BroadcastReceiver
{
    private static final String TAG = "SerialTest.RepeatingAlarm";

    @Override
    public void onReceive(Context context, Intent intent)
    {
	PowerManager pm;
	PowerManager.WakeLock wl;

        Log.v(TAG,"Alarm received");

	pm = (PowerManager)context.getSystemService(Context.POWER_SERVICE);

	/* turn screen ON */
	wl = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP,"serialtest");
	wl.acquire();

	Log.v(TAG,"Screen ON");

	Intent i = new Intent(StartApplication.ACTION_ALARM);
	context.sendBroadcast(i);

	try{
	    Thread.sleep(2000);
	}catch (Exception e){
	    Log.v(TAG, "Could not sleep: " + e);
	}

	/* allow screen to be turned back OFF */
	wl.release();

	Log.v(TAG,"Screen OFF");

    }
}
