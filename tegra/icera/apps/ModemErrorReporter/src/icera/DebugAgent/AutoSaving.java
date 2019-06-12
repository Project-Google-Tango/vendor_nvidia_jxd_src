// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;

import java.util.ArrayList;
import android.view.View;
import android.widget.ToggleButton;

public class AutoSaving extends ListMenu{
    public void setWidgetData(){
        mStr = new String[]{
                "Unrecoverable network lost", "Network lost", "Data stall", "SIM lost",
                "Call drop", "APP crash"
        };
        mBl = new boolean[]{
                mPrefs.isUnrecoverNetworkLostEnabled(), mPrefs.isNetworkLostEnabled(), mPrefs.isDataStallEnabled(), mPrefs.isSimLostEnabled(),
                mPrefs.isCallDropEnabled(), mPrefs.isAppCrashEnabled()
        };

        togguleListener = new ArrayList<IClkListener>();
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                unrecoverNetWorkLost(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                netWorkLost(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                dataStall(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                simLost(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                callDrop(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                appCrash(view);
            }
        });
    }

    public void netWorkLost(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setNetworkLostEnabled(btn.isChecked());
    }

    public void dataStall(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setDataStallEnabled(btn.isChecked());
    }

    public void simLost(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setSimLostEnabled(btn.isChecked());
    }

    public void callDrop(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setCallDropEnabled(btn.isChecked());
    }

    public void appCrash(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setAppCrashEnabled(btn.isChecked());
    }

    public void unrecoverNetWorkLost(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setUnrecoverNetworkLostEnabled(btn.isChecked());
    }
}
