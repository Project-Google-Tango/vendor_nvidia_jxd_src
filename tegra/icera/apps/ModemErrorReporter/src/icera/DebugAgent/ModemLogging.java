// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
package icera.DebugAgent;

import android.content.Context;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;
import android.widget.AdapterView.OnItemSelectedListener;

public class ModemLogging extends ListMenu {
    private static final String TAG = "ModemLogging";
    private long mLoggingSize = 200; /* in Mo bytes, size 0 means 'No limit' */

    // private static final int RESERVED_SPACE_SIZE = 100; /* in MB */

    private int mLogSizePos = 0;
    private EditText mDeferredTimeout;
    private EditText mDeferredWatermark;

    public void setWidgetData(){    
    }
    
    public void initWidget(){
        final ArrayAdapter<CharSequence> adapter;
        adapter = ArrayAdapter.createFromResource(this,
                R.array.PossibleSizes,
                android.R.layout.simple_spinner_item);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mLogSizePos = mPrefs.getLogSizePos();
        mLoggingSize = mPrefs.getLoggingSize();
        
        String[] str = new String[]{
                getString(R.string.TextLoggingSize), getString(R.string.ZipSaved), getString(R.string.DeferredLogging),
                getString(R.string.DeferredTimeout), getString(R.string.DeferredWatermark)
                };
        int[] id = new int[]{
                R.id.textView1, R.id.testItem2, R.id.testItem2, R.id.textView3, R.id.textView3
        };
          ItemWidget<?>[] wd1;
        wd1 = new ItemWidget<?>[str.length];

        int i = 0;
        for (String name: str){
            wd1[i] = new ItemWidget<TextView>(id[i], name, false){
                @Override
                public void setView(View parent){
                    this.mView = (TextView)parent.findViewById(this.getId());
                    this.mView.setText(this.getName());
                }
            };
            i++;
        }
        ItemWidget<?> wd = new ItemWidget<Spinner>(R.id.spinner1, null, false){
            @Override
            public void setView(View parent){
                this.mView = (Spinner)parent.findViewById(this.getId());
                this.mView.setAdapter(adapter);
                this.mView.setSelection(mLogSizePos);
                this.mView.setOnItemSelectedListener(new SetLogSizeListener(ModemLogging.this));
                this.mView.setEnabled(!mPrefs.isModemLoggingEnabled());
            }
        }; 
        TestItem itm = new TestItem("", R.layout.spinner_item, false);
        itm.addItem(wd1[0]);
        itm.addItem(wd);
        items.add(itm);
        wd = new ItemWidget<ToggleButton>(R.id.testItemButton2, "", mPrefs.isCompressBeanieEnabled()){
            @Override
            public void setView(View parent){
                this.mView = (ToggleButton)parent.findViewById(this.getId());
                this.setChecked(mPrefs.isCompressBeanieEnabled());
                this.mView.setChecked(this.isChecked());
                if (mPrefs.isModemLoggingEnabled()){
                    this.mView.setEnabled(false);
                    this.mView.setTextColor(0xFFFFFFFF);
                }
            }
        };
        wd.setClkListener(new IClkListener(){
            @Override
            public void handleClick(View view) {
                compressBeanieSwitch(view);
            }
        });
        itm = new TestItem("", R.layout.item_logcat, false);
        itm.addItem(wd1[1]);
        itm.addItem(wd);
        items.add(itm);
        wd = new ItemWidget<ToggleButton>(R.id.testItemButton2, "", mPrefs.isDeferredLoggingEnabled()){
            @Override
            public void setView(View parent){
                this.mView = (ToggleButton)parent.findViewById(this.getId());
                this.setChecked(mPrefs.isDeferredLoggingEnabled());
                this.mView.setChecked(this.isChecked());
                if (mPrefs.isModemLoggingEnabled()){
                    this.mView.setEnabled(false);
                    this.mView.setTextColor(0xFFFFFFFF);
                }
            }
        };
        wd.setClkListener(new IClkListener(){
            @Override
            public void handleClick(View view) {
                deferredLoggingSwitch(view);
            }
        });
        itm = new TestItem("", R.layout.item_logcat, false);
        itm.addItem(wd1[2]);
        itm.addItem(wd);
        items.add(itm);
        
        wd = new ItemWidget<EditText>(R.id.editText1, mPrefs.getDeferredTimeoutPos() + "", true){
            @Override
            public void setView(View parent){
                this.mView = (EditText)parent.findViewById(this.getId());
                if (this.mView.getText().toString().equals("")){
                    this.mView.setText(this.getName());
                    this.mView.requestFocusFromTouch();
                }
                if (mPrefs.isModemLoggingEnabled()){
                    this.mView.setEnabled(false);
                    this.mView.setTextColor(0xFFFFFFFF);
                }
                mDeferredTimeout = this.mView;
            }
        };
        itm = new TestItem("", R.layout.edit_item, false);
        itm.addItem(wd1[3]);
        itm.addItem(wd);
        items.add(itm);
        
        wd = new ItemWidget<EditText>(R.id.editText1, mPrefs.getDeferredWatermarkPos() + "", true){
            @Override
            public void setView(View parent){
                this.mView = (EditText)parent.findViewById(this.getId());
                if (this.mView.getText().toString().equals("")){
                    this.mView.setText(this.getName());
                    this.mView.requestFocusFromTouch();
                }
                //this.mView.setText(this.getName());
                if (mPrefs.isModemLoggingEnabled()){
                    this.mView.setEnabled(false);
                    this.mView.setTextColor(0xFFFFFFFF);
                }
                mDeferredWatermark = this.mView;
            }
        };
        itm = new TestItem("", R.layout.edit_item, false);
        itm.addItem(wd1[4]);
        itm.addItem(wd);
        items.add(itm);
    }
    
    private class SetLogSizeListener implements OnItemSelectedListener {


        private String maxSpaceStr = null;

        public SetLogSizeListener(Context ctx) {
            super();

            maxSpaceStr = ctx.getResources().getStringArray(R.array.PossibleSizes)[4];
        }

        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {

            // Avoid the false warning message (NVBUG1268665)
            if (!parent.isEnabled())
                return;

            String selectedSize = parent.getItemAtPosition(pos).toString();
            Log.d(TAG, "selectedSize = " + selectedSize);

            long freeSpace;

            if (mPrefs.isLoggingOnSDCard())
                freeSpace = Environment.getExternalStorageDirectory().getFreeSpace();
            else
                freeSpace = Environment.getDataDirectory().getFreeSpace();

            freeSpace -= LogUtil.FULL_COREDUMP_FILE_SIZE * 3
                    + (SaveLogs.RESERVED_SPACE_SIZE * 1024 * 1024);

            if (!selectedSize.contains(maxSpaceStr)
                    && (Integer.parseInt(selectedSize.split("Mo")[0]) * 1024 * 1024) >= freeSpace) {
                parent.setSelection(mLogSizePos);
                Toast.makeText(ModemLogging.this, "Not enough free space!", Toast.LENGTH_SHORT).show();
                Toast.makeText(
                        ModemLogging.this,
                        (Integer.parseInt(selectedSize.split("Mo")[0]) - freeSpace / (1024 * 1024))
                                + "Mo missing on the SDcard", Toast.LENGTH_SHORT).show();
                Toast.makeText(ModemLogging.this, "Delete old logs or change the log size",
                        Toast.LENGTH_SHORT).show();
                return;
            } else if (selectedSize.contains(maxSpaceStr)
                    && (freeSpace <= (SaveLogs.DEFAULT_IOM_SIZE * 1024 * 1024)
                            * SaveLogs.MINIMUM_NUM_OF_IOM)) {
                // Jacky's zipping logger needs
                // at least 2 iom files, but icera_log_serial_arm will behave
                // differently with 2 iom files.
                // so use a minimum of 4 iom files.
                Toast.makeText(ModemLogging.this, "Not enough free space!", Toast.LENGTH_SHORT).show();
                Toast.makeText(
                        ModemLogging.this,
                        (SaveLogs.DEFAULT_IOM_SIZE * SaveLogs.MINIMUM_NUM_OF_IOM - freeSpace / (1024 * 1024))
                                + "Mo missing on the SDcard", Toast.LENGTH_SHORT).show();
                Toast.makeText(ModemLogging.this, "Delete old logs or change the log size",
                        Toast.LENGTH_SHORT).show();
        parent.setSelection(mLogSizePos);
                return;
            }

            if (!selectedSize.contains(maxSpaceStr)) {
                mLoggingSize = Integer.parseInt(selectedSize.split("Mo")[0]);
                mPrefs.setNoLimit(false);
            } else {
                Toast.makeText(ModemLogging.this,
                        "Pay attention : Free space = " + freeSpace / (1024 * 1024) + "Mo",
                        Toast.LENGTH_SHORT).show();
                mPrefs.setNoLimit(true);
                mLoggingSize = (int)((freeSpace / (1024 * 1024)) / SaveLogs.DEFAULT_IOM_SIZE)
                        * SaveLogs.DEFAULT_IOM_SIZE; // Round
                // to
                // multiple
                // 200MB
            }
            mPrefs.setLoggingSize(mLoggingSize);

            mLogSizePos = pos;
            mPrefs.setLogSizePos(mLogSizePos);

            Log.d(TAG, "Logging size changed to " + mLoggingSize + "Mo");
        }

        public void onNothingSelected(AdapterView<?> parent) {
            // Do nothing.
        }
    }
  
    public void compressBeanieSwitch(View view) {
        ToggleButton btn = (ToggleButton)view;
        if (btn.isChecked()) {
            mPrefs.setCompressBeanieEnabled(true);
            Toast.makeText(this, "Enable Compress Beanie", Toast.LENGTH_SHORT).show();
        } else {
            mPrefs.setCompressBeanieEnabled(false);
            Toast.makeText(this, "Disable Compress Beanie", Toast.LENGTH_SHORT).show();
        }
    }

    public void deferredLoggingSwitch(View view) {
        ToggleButton btn = (ToggleButton)view;
        boolean bl = mPrefs.isBeaniePermission();
        if (!bl){
            Toast.makeText(this, "icera_log_serial_arm can't be executed.\nPlease check the permission!", Toast.LENGTH_LONG).show();
            return;
        }
        bl = mPrefs.isBeanieVersionOld();
        if (bl){
            Toast.makeText(this, "icera_log_serial_arm don't support deferred logging.\nIf you want deferred logging, please check and update\n /system/bin/icera_log_serial_arm\n and restart IDA\n", Toast.LENGTH_LONG).show();
            btn.setChecked(false);
            return;
        }            
        if (btn.isChecked()) {
            mPrefs.setDeferredLoggingEnabled(true);
            Toast.makeText(this, "Enable Deferred Logging", Toast.LENGTH_SHORT).show();
        } else {
            mPrefs.setDeferredLoggingEnabled(false);
            Toast.makeText(this, "Disable Deferred Logging", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    public void onBackPressed() {
        if (Integer.parseInt(mDeferredWatermark.getText().toString()) > 100){
            Toast.makeText(this, "Deferred Watermark should be less than 100", Toast.LENGTH_SHORT).show();
            return;
        }
        mPrefs.setDeferredTimeoutPos(Integer.parseInt(mDeferredTimeout.getText().toString()));
        mPrefs.setDeferredWatermarkPos(Integer.parseInt(mDeferredWatermark.getText().toString()));
        Log.d(TAG, mDeferredTimeout.getText().toString());
        Log.d(TAG, mDeferredWatermark.getText().toString());
        this.finish();
    }
}
