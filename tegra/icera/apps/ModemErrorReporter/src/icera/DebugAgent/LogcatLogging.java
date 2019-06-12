// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;


import java.util.ArrayList;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

public class LogcatLogging extends ListMenu {
    private int indexOfLogcat = 3;
    public void setWidgetData(){
        mStr = new String[]{
                getString(R.string.LastKmsg), getString(R.string.APBugReport), getString(R.string.TCPDump),
                getString(R.string.LogcatMain), getString(R.string.LogcatSystem),
                getString(R.string.LogcatEvent), getString(R.string.LogcatRadio), getString(R.string.LogcatAll)
        };
        mBl = new boolean[]{
                mPrefs.isLastKmsgEnabled(), mPrefs.isBugreportEnabled(), mPrefs.isTCPDumpEnabled(),
                mPrefs.isLogcatMainEnabled(), mPrefs.isLogcatSystemEnabled(),
                mPrefs.isLogcatEventsEnabled(), mPrefs.isLogcatRadioEnabled(), mPrefs.isLogcatAllEnabled()
        };

        togguleListener = new ArrayList<IClkListener>();
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                lastKmsg(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                apBugReport(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                tcpDump(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                logcatMain(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                logcatSystem(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                logcatEvents(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                logcatRadio(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                logcatAll(view);
            }
        });
    }
    public void initWidget(){
        if (mPrefs.isLogcatAllEnabled()){
            mPrefs.setLogcatMainEnabled(false);
            mPrefs.setLogcatSystemEnabled(false);
            mPrefs.setLogcatEventsEnabled(false);
            mPrefs.setLogcatRadioEnabled(false);
        }
        setWidgetData();
        if (mStr.length == 0 || mStr.length != mBl.length || mStr.length != togguleListener.size())
            return;

        ItemWidget<?>[] wd1, wd2;
        wd1 = new ItemWidget<?>[mStr.length];
        wd2 = new ItemWidget<?>[mBl.length];

         TestItem itm;
        for (int i = 0; i < mStr.length; i++){
            wd1[i] = new ItemWidget<TextView>(getTestItemId(), mStr[i], false){
                @Override
                public void setView(View parent){
                    //super.getView(parent);
                    this.mView = (TextView)parent.findViewById(this.getId());
                    this.mView.setText(this.getName());
                }
            };
            // for logcatmain, logcatevent, logcatsystem, logcatradio items, the switchs depend on the logcatAll items.
            if (i > indexOfLogcat-1 && i < mStr.length - 1)
            {
                wd2[i] = new ItemWidget<ToggleButton>(getTestItemButtonId(), null, mBl[i]){
                @Override
                public void setView(View parent){
                    //super.getView(parent);
                    this.mView = (ToggleButton)parent.findViewById(this.getId());
                    this.mView.setChecked(this.isChecked());
                    if (mPrefs.isLogcatAllEnabled())
                        this.mView.setEnabled(false);
                        this.mView.setTextColor(0xffffffff);
                    }
                };
            }
            else
            {
                wd2[i] = new ItemWidget<ToggleButton>(getTestItemButtonId(), null, mBl[i]){
                    @Override
                    public void setView(View parent){
                        //super.getView(parent);
                        this.mView = (ToggleButton)parent.findViewById(this.getId());
                        this.mView.setChecked(this.isChecked());
                        }
                    };
            }
            wd2[i].setClkListener(togguleListener.get(i));

            itm = new TestItem(mStr[i], getTestItemLayoutId(), false);
            itm.addItem(wd1[i]);
            itm.addItem(wd2[i]);
            items.add(itm);
        }
    }
    
    public void lastKmsg(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setLastKmsgEnabled(btn.isChecked());
    }

    public void apBugReport(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setBugreportEnabled(btn.isChecked());
    }

    public void tcpDump(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setTCPDumpEnabled(btn.isChecked());

        Toast.makeText(this, "Not implemented yet", Toast.LENGTH_SHORT).show();
    }

    public void kernelLogging(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setKernelLoggingEnabled(btn.isChecked());

        Toast.makeText(this, "Not implemented yet", Toast.LENGTH_SHORT).show();
    }
    
    public void logcatAll(View view) {
        ToggleButton btn = (ToggleButton)view;
        if (btn.isChecked()) {
            mPrefs.setLogcatAllEnabled(true);
            setAllLogcatItemsEnable(false);
        } 
        else {
            mPrefs.setLogcatAllEnabled(false);
            setAllLogcatItemsEnable(true);
        }
    }
    
    public void logcatMain(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setLogcatMainEnabled(btn.isChecked());
    }

    public void logcatSystem(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setLogcatSystemEnabled(btn.isChecked());
    }

    public void logcatEvents(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setLogcatEventsEnabled(btn.isChecked());
    }

    public void logcatRadio(View view) {
        ToggleButton btn = (ToggleButton)view;
        mPrefs.setLogcatRadioEnabled(btn.isChecked());
    }
    
    // for logcatmain, logcatevent, logcatsystem, logcatradio items, the switchs depend on the logcatAll items.
    public void setAllLogcatItemsEnable(boolean bl){

        for (int i = indexOfLogcat; i < items.size() - 1; i++){
            for (int j = 0; j < items.get(i).mItems.size(); j++){
                if (items.get(i).mItems.get(j).getId() == R.id.testItemButton2){
                    ToggleButton v = (ToggleButton)items.get(i).mItems.get(j).mView;
                    if (!bl){
                        v.setChecked(bl);
                        mPrefs.setLogcatMainEnabled(false);
                        mPrefs.setLogcatSystemEnabled(false);
                        mPrefs.setLogcatEventsEnabled(false);
                        mPrefs.setLogcatRadioEnabled(false);
                        v.setTextColor(0xffffffff);
                    }
                    v.setEnabled(bl);
                }
            }
        }
    }
}
