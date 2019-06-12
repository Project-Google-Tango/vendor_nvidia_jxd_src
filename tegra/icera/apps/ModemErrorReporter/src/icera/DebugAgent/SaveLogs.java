// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import icera.DebugAgent.BugReportService.BugReportServiceBinder;
import icera.DebugAgent.BugReportService.NotificationType;
import icera.DebugAgent.R.id;
import icera.DebugAgent.R;
import android.app.AlertDialog;
import android.content.*;
import android.graphics.Color;
import android.os.*;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.*;
import android.widget.AdapterView.OnItemClickListener;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.URLSpan;
import android.util.Log;
import java.io.*;
import java.util.*;
import java.lang.Process;



public class SaveLogs extends ListMenu {

    static final String TAG = "SaveLogs";

    public static final String invalidChars = "\\/:*?\"<>|";

    public static final int NORMAL = 0;

    public static final int MODEM_LOGGING_ISSUE = 1;

    public static final int MODEM_AUTO_SAVING = 2;

    private static final int MODEM_SETTING_RETURN = 0;
    
    private static final int LOGCAT_SETTING_RETURN = 1;
    
    private static final int AUTO_SAVING_SETTING_RETURN = 2;

    private static final int CLEAN_LOGS_RETURN = 3;
        
    public static final int DEFAULT_IOM_SIZE = 100; /* in MB */

    public static final int RESERVED_SPACE_SIZE = 100; /* in MB */

    public static final int MINIMUM_NUM_OF_IOM = 4;

    // Binding to BugReportService
    BugReportService mService;

    LoggerManager mLoggerMgr;

    boolean mBound = false;

    Process marker = null;

    // Output directory name
    static String mLogSaveDir = null;

    final int LOGCAT_HANDLED = 100;
    final int M_NOTIFY_NORMAL = 101;
    final int AUTOSAVING_HANDLED = 102;
    final int TOGGLEBUTTON_HANDLED = 103;
    private View mAPSwitchButton = null, mAutoSavingSwitchButton = null;

    Handler mHandler = new Handler(){
        public void handleMessage (Message msg)
        {
            switch(msg.what)
            {
                case LOGCAT_HANDLED:
                    handleLogcatLoggingState(true);
                    Log.d(TAG, "The handler thread id = " + Thread.currentThread().getId() + " ");
                    break;
                case AUTOSAVING_HANDLED:
                    handleAutoSavingConfig(true);
                    Log.d(TAG, "The handler thread id = " + Thread.currentThread().getId() + " ");
                    break;
                case M_NOTIFY_NORMAL:
                    mService.notifyMessage(BugReportService.NotificationType.NOTIFY_NORMAL);
                    break;
                case TOGGLEBUTTON_HANDLED:
                    if (mAPSwitchButton == null)
                        mAPSwitchButton = findSwitchView(getString(R.string.APLogging));
                    if (mAPSwitchButton != null)
                        mAPSwitchButton.setEnabled(false);
                    if (mAutoSavingSwitchButton == null)
                        mAutoSavingSwitchButton = findSwitchView(getString(R.string.AutoSaving));
                    if (mAutoSavingSwitchButton != null)
                        mAutoSavingSwitchButton.setEnabled(false);
                    break;
            }
        }
    };

    /** Defines callback for service binding, passed to bindService() */
    private ServiceConnection mConnection = new ServiceConnection() {

        public void onServiceConnected(ComponentName className, IBinder service) {
            // We've bound to LocalService, cast the IBinder and get
            // LocalService instance
            BugReportServiceBinder binder = (BugReportServiceBinder)service;
            mService = binder.getService();
            mLoggerMgr = mService.getLoggerManager();
            mBound = true;
            Log.d(TAG, "onServiceConnected");
        }

        public void onServiceDisconnected(ComponentName arg0) {
            mService = null;
            mLoggerMgr = null;
            Log.d(TAG, "onServiceDisconnected");
        }
    };

    private int mNotifyType;

    /**
     * Called at creation of the activity - display layout - bind to service
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {

        Log.d("Icera", "Creating view");
        super.onCreate(savedInstanceState);
        // Reset comment pane
        EditText text = (EditText)findViewById(R.id.MarkerComment);
        text.setText(mPrefs.getUserMarkerString());

        // Reset comment pane
        EditText text2 = (EditText)findViewById(R.id.Comment);
        text2.setText("");
        
        initPrefs();        
        mNotifyType = getIntent().getIntExtra("NotificationType", NORMAL);
        if (savedInstanceState != null)
            mNotifyType = savedInstanceState.getInt("NotificationType");
        Log.d(TAG, "notif type = " + mNotifyType);

        mLogSaveDir = generateSaveDir(mNotifyType);
        if (mLogSaveDir != null){
            Log.d(TAG, "Log Saved in directory: " + mLogSaveDir);
            // Display default log name
            EditText text3 = (EditText)findViewById(R.id.Dirname);
            text3.setText(mLogSaveDir);
        }
        manageLogsDir();

        // Bind to service
        Intent intent = new Intent(this, BugReportService.class);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

        Log.d("Icera", "View SaveLogs created");
 
        lv.setOnItemClickListener(new OnItemClickListener(){

            @Override
            public void onItemClick(AdapterView<?> arg0, View arg1, int arg2,
                    long arg3) {
                // TODO Auto-generated method stub
                items.get(arg2).handleClkEvent(arg1);
            }
        });
        mHandler.sendEmptyMessageDelayed(TOGGLEBUTTON_HANDLED, 500);
    }

    private void initPrefs() {
  //      mPrefs = new Prefs(this);
  //      mPrefs.setBeanieVersionOld(!supportDeferredFeature());
        if (mPrefs.isBeanieVersionOld())
            mPrefs.setDeferredLoggingEnabled(false);
        //parseCommandLine();
    }

    public String generateSaveDir(int notifyType){
        String str = null;
        // Construct from current time
        String currTime = LogUtil.LOG_DATE_FORMAT.format(new Date());
        Message msg = new Message();
        switch (notifyType) {
            case MODEM_LOGGING_ISSUE:
                str = currTime + "_LOGS";
                //headMsg = "Modem error detected";        
                msg.what = M_NOTIFY_NORMAL;
                mHandler.sendMessageDelayed(msg , 1500);
                break;

            case MODEM_AUTO_SAVING:
                //add code here
                str = currTime + "_LOGS";
                msg.what = M_NOTIFY_NORMAL;
                mHandler.sendMessageDelayed(msg , 1500);
                break;

            case NORMAL:
                str = currTime + "_LOGS";
                //headMsg = "No error detected";
                break;
        }
        return str;
    }


    /**
     * Called when stopping the activity - unbind service
     */
    @Override
    protected void onStop() {
        super.onStop();
        // Unbind from the service
        if (mBound) {
            unbindService(mConnection);
            mBound = false;
            Log.d(TAG, "onStop");
        }
    }

    @Override
    protected void onNewIntent(Intent i) {

        mNotifyType = i.getIntExtra("NotificationType", NORMAL);

        Log.d(TAG, "New intent received, extra = " + mNotifyType);
        Message msg = new Message();
        //String headMsg = "No error detected";
        switch (mNotifyType) {
            case MODEM_LOGGING_ISSUE:
                //headMsg = "Modem error detected";
                msg.what = M_NOTIFY_NORMAL;
                mHandler.sendMessage(msg);
                break;

            case MODEM_AUTO_SAVING:
                msg.what = M_NOTIFY_NORMAL;
                mHandler.sendMessage(msg);
                break;
        }

    }

    /*
     * (non-Javadoc)
     * @see android.app.Activity#onSaveInstanceState(android.os.Bundle)
     */
    @Override
    protected void onSaveInstanceState(Bundle outState) {

        super.onSaveInstanceState(outState);

        outState.putInt("NotificationType", mNotifyType);
    }

    /* callback for 'Save logs' button */
    public void storeLogs(View view) {

        // Save comment if something is entered in textbox.
        EditText text = (EditText)findViewById(id.Comment);
        String comment = text.getText().toString();

        // Copy the logs.
        // Get the directory name in case the user modified it
        EditText textDir = (EditText)findViewById(id.Dirname);
        mLogSaveDir = textDir.getText().toString();
        char[] chars = mLogSaveDir.toCharArray();
        for (int i = 0; i < chars.length; i++) {
            if ((invalidChars.indexOf(chars[i])) >= 0) {
                Toast.makeText(this, "Invalid character (\\/:*?\"<>|) found in folder name", Toast.LENGTH_SHORT).show();
                return;
            }
        }
        mLoggerMgr.saveComment(comment, mLogSaveDir);

        if (mLogSaveDir.contentEquals("")) {
            Toast.makeText(this, "Log store folder name cannot be empty", Toast.LENGTH_SHORT).show();
            return;
        } else {
            mService.storeLogsAsync(mLogSaveDir, mNotifyType == NORMAL ? false : true);
            this.finish();
        }
    }

    /* callback for 'Insert Marker' button */
    public void insertMarker(View view) {

        EditText text = (EditText)findViewById(R.id.MarkerComment);
        String comment = text.getText().toString();
        mPrefs.setUserMarkerString(comment);

        Date date = new Date();
        String Time = date.toString();
        String[] temp = Time.split(" ");
        String[] temp2 = temp[3].split(":");
        String Thetime = "_" + temp[1] + "-" + temp[2] + "_" + temp2[0] + ":" + temp2[1] + ":"
                + temp2[2] + "_";
        String command = "AT%IMARKER=MARKER" + Thetime + comment + "\r";
        String commandToModem = "echo" + " " + "\"" + command + "\"" + " > " + getATPort();
        // display marker in logcat main buffer
        Log.d(TAG, commandToModem);

        // display marker in the genie log
        if (marker != null)
            marker.destroy();

        try {
            marker = Runtime.getRuntime()
                    .exec(new String[] {
                            "/system/bin/sh", "-c", commandToModem
                    });
            Toast.makeText(this, "MARKER ADDED : " + command, Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }
    /*
     * Add the function for the compatibility with the old BSP version before.
     */
    static public String getATPort()
    {
        final String[] str= {"/dev/at_modem", "/dev/ttyACM2", "/dev/ttySHM2"};
        for(String fileName: str){
            File file = new File(fileName);
            if (file.exists())
                return fileName;
        }
        return null;
    }

    /* callback for 'Delete logs' button */
    public void discardLogs(View view) {
        // TODO: remove useless logs
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage("Are you sure you want to discard logs ?").setCancelable(false)
                .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        mService.notifyMessage(NotificationType.NOTIFY_DISCARD);
                        mLoggerMgr.restartLogging();
                        finish();
                    }
                }).setNegativeButton("No", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        dialog.cancel();
                    }
                });
        builder.show();
    }

    public void modemLoggingSwitch(View view) {
        ToggleButton btn = (ToggleButton)view;
        if (btn.isChecked() && mPrefs.isModemLoggingEnabled() == false) {

            boolean result = mLoggerMgr.startMdmLogging();

            if (result == false) {
                // failed to start Beanie logging
                Log.e(TAG, "Failed to start Beanie logging");
                btn.setChecked(false);

                Toast.makeText(this, "Error while starting logging", Toast.LENGTH_SHORT).show();
                Toast.makeText(this, "check icera_log_serial_arm tool under /system/bin/",
                        Toast.LENGTH_SHORT).show();
                Toast.makeText(this, "and restart the phone", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "Modem logging started", Toast.LENGTH_SHORT).show();

                mService.notifyMessage(BugReportService.NotificationType.NOTIFY_NORMAL);

            }
        } else if (mPrefs.isModemLoggingEnabled() == true) {
            mLoggerMgr.stopMdmLogging();
            Toast.makeText(this, "Modem logging stopped", Toast.LENGTH_SHORT).show();
        }
    }

    public void autoLaunchSwitch(View view) {

        ToggleButton btn = (ToggleButton)view;
        if (btn.isChecked()) {
            mPrefs.setAutoLaunchEnabled(true);
            Toast.makeText(this, "Auto Launch after power on", Toast.LENGTH_SHORT).show();
        } else {
            mPrefs.setAutoLaunchEnabled(false);
            Toast.makeText(this, "Don't Launch after power on", Toast.LENGTH_SHORT).show();
        }
    }

    public void logcat(View view) {
        /*ToggleButton btn = (ToggleButton)view;
        if (btn.isChecked()) {
            mPrefs.setLogcatEnabled(true);
            handleLogcatLoggingState(true);
            Toast.makeText(this, "Logcat Logging started", Toast.LENGTH_SHORT).show();
        } else {
            mPrefs.setLogcatEnabled(false);
            handleLogcatLoggingState(false);
            Toast.makeText(this, "Logcat Logging stopped", Toast.LENGTH_SHORT).show();
        }*/
    }
    
    public void autoSaving(View view) {
        ToggleButton btn = (ToggleButton)view;
        handleAutoSavingConfig(btn.isChecked());
        mPrefs.setAutoSavingEnabled(btn.isChecked());
    }

    @Override
    protected void onActivityResult (int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);
        Message msg = new Message();
        switch (requestCode){
            case LOGCAT_SETTING_RETURN:
                msg.what = LOGCAT_HANDLED;
                mHandler.sendMessageDelayed(msg , 50);
                break;
            case AUTO_SAVING_SETTING_RETURN:
                msg.what = AUTOSAVING_HANDLED;
                mHandler.sendMessageDelayed(msg , 50);
                break;
            case  MODEM_SETTING_RETURN:
                break;
                default:
                    break;
        }
    }

    @Override
    protected void onRestart(){
        super.onRestart();
        mLogSaveDir = generateSaveDir(mNotifyType);
        if (mLogSaveDir != null){
            Log.d(TAG, "Log Saved in directory: " + mLogSaveDir);
            // Display default log name
            EditText text3 = (EditText)findViewById(R.id.Dirname);
            text3.setText(mLogSaveDir);
        }
    }

    protected void handleLogcatLoggingState(boolean logcatEnabled){
        mPrefs.setLogcatEnabled(mPrefs.isLogcatAllEnabled() || mPrefs.isLogcatMainEnabled() ||
                mPrefs.isLogcatSystemEnabled() || mPrefs.isLogcatEventsEnabled() || mPrefs.isLogcatRadioEnabled() ||
                mPrefs.isLastKmsgEnabled() || mPrefs.isBugreportEnabled() || mPrefs.isTCPDumpEnabled());
        if (mAPSwitchButton != null)
            ((ToggleButton) mAPSwitchButton).setChecked(mPrefs.isLogcatEnabled());
        if (logcatEnabled){
            if (mPrefs.isLogcatAllEnabled()){
                mLoggerMgr.startLogcat();
            }
            else{
                mLoggerMgr.stopLogcat();
            }
            if (mPrefs.isLogcatMainEnabled()){
                mLoggerMgr.startLogcatMain();
            }
            else{
                mLoggerMgr.stopLogcatMain();
            }
            if (mPrefs.isLogcatSystemEnabled()){
                mLoggerMgr.startLogcatSystem();
            }
            else{
                mLoggerMgr.stopLogcatSystem();
            }
            if (mPrefs.isLogcatEventsEnabled()){
                mLoggerMgr.startLogcatEvents();
            }
            else{
                mLoggerMgr.stopLogcatEvents();
            }
            if (mPrefs.isLogcatRadioEnabled()){
                mLoggerMgr.startLogcatRadio();
            }
            else{
                mLoggerMgr.stopLogcatRadio();
            }
        }
        else{
            mLoggerMgr.stopLogcat();
            mLoggerMgr.stopLogcatMain();
            mLoggerMgr.stopLogcatSystem();
            mLoggerMgr.stopLogcatEvents();
            mLoggerMgr.stopLogcatRadio();
        }
    }

    public void handleAutoSavingConfig(boolean bl){
        mPrefs.setAutoSavingEnabled(mPrefs.isNetworkLostEnabled() || mPrefs.isDataStallEnabled() ||
                mPrefs.isSimLostEnabled() || mPrefs.isCallDropEnabled() || mPrefs.isUnrecoverNetworkLostEnabled() ||
                mPrefs.isAppCrashEnabled());
        if (mAutoSavingSwitchButton != null)
            ((ToggleButton) mAutoSavingSwitchButton).setChecked(mPrefs.isAutoSavingEnabled());
        mService.runAutoSavingMonitor(bl);
    }

    public void setWidgetData(){
        mStr = new String[]{
                getString(R.string.AutoStart), getString(R.string.ModemLogging), getString(R.string.APLogging),
                //getString(R.string.LastKmsg), getString(R.string.APBugReport), getString(R.string.TCPDump), getString(R.string.AutoSaving)
                getString(R.string.AutoSaving)
        };
        mBl = new boolean[]{
                mPrefs.isAutoLaunchEnabled(), mPrefs.isModemLoggingEnabled(), mPrefs.isLogcatEnabled(),
                //mPrefs.isLastKmsgEnabled(), mPrefs.isBugreportEnabled(), false, mPrefs.isAutoSavingEnabled()
                mPrefs.isAutoSavingEnabled()
        };

        togguleListener = new ArrayList<IClkListener>();
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                autoLaunchSwitch(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                modemLoggingSwitch(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                logcat(view);
            }
        });
        togguleListener.add(new IClkListener(){
            @Override
            public void handleClick(View view) {
                autoSaving(view);
            }
        });
    }
    public void initWidget(){
        super.initWidget();
        for (TestItem itm : items){
            if (itm.getName().equals(getString(R.string.ModemLogging))){
                itm.isFocusable = true;
                itm.setClkListener(new IClkListener(){
                    @Override
                    public void handleClick(View view) {
                        startActivityForResult(new Intent (SaveLogs.this, ModemLogging.class), MODEM_SETTING_RETURN);
                    }
                });
            }
            else if (itm.getName().equals(getString(R.string.APLogging))){
                itm.isFocusable = true;
                itm.setClkListener(new IClkListener(){
                    @Override
                    public void handleClick(View view) {
                        startActivityForResult(new Intent (SaveLogs.this, LogcatLogging.class), LOGCAT_SETTING_RETURN);
                    }
                });
            }
            else if (itm.getName().equals(getString(R.string.AutoSaving))){
                itm.isFocusable = true;
                itm.setClkListener(new IClkListener(){
                    @Override
                    public void handleClick(View view) {
                        startActivityForResult(new Intent (SaveLogs.this, AutoSaving.class), AUTO_SAVING_SETTING_RETURN);
                    }
                });
            }
        }
    }

    @Override
    public int getLayoutId(){
        setLayoutId(R.layout.savelogs2);
        return super.getLayoutId();
    }
    
    @Override
    public int getListViewId(){
        setListViewId(R.id.listview1);
        return super.getListViewId();
    }
    
    @Override
    public int getTestItemId(){
        setTestItemId(R.id.testItem1);
        return super.getTestItemId();
    }
    
    @Override
    public int getTestItemButtonId(){
        setTestItemButtonId(R.id.testItemButton1);
        return super.getTestItemButtonId();
    }
    
    @Override
    public int getTestItemLayoutId(){
        setTestItemLayoutId(R.layout.item_main);
        return super.getTestItemLayoutId();
    }

    @Override
    public void setLvFocus(boolean focus){
    }

    public void manageLogsDir(){
        TextView txtV = (TextView) findViewById(R.id.textView12);
        SpannableString sp = new SpannableString(getString(R.string.LogPath));
        sp.setSpan(new URLSpan(""), 0, getString(R.string.LogPath).length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE); 
        sp.setSpan(new ForegroundColorSpan(Color.GREEN), 0, getString(R.string.LogPath).length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE); 
        txtV.setOnClickListener(new OnClickListener(){
            @Override
            public void onClick(View arg0) {
                // TODO Auto-generated method stub
                startActivityForResult(new Intent (SaveLogs.this, DisplayLogs.class), CLEAN_LOGS_RETURN);
            }
        });
        txtV.setText(sp);
    }
    public View findSwitchView(String name){
        for (TestItem itm : items){
            if (itm.getName().equals(name)){
                for (ItemWidget<?> wd: itm.mItems){
                    if (wd.getId() == R.id.testItemButton1) {
                        //if (wd.mView != null)
                        //wd.mView.setEnabled(false);
                        return wd.mView;
                    }
                }
            }
        }
        return null;
    }
}
