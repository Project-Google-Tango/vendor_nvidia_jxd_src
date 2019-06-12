// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;

import android.content.Context;
import android.os.Environment;
import android.util.Log;
import android.widget.Toast;

public class ModemLogger extends Logger {

    public static final String TAG = "ModemLog";

    public static final String MODEM_LOGGING_DEV = "/dev/log_modem";

    private static final String GENIE_TOOLS_DIRECTORY = "tools"; // System will
                                                                 // add "app_"
                                                                 // prefix

    private static final String GENIE_IOS_TOOL = "genie-ios-arm";

    private static final String GENIE_LOGGING_TOOL = "icera_log_serial_arm";

    private static final String MODEM_DEBUG_DIRECTORY = "/data/rfs/data/debug";

    private static final String IOS_FILE_NAME = "dyn_filter.ios";

    private static final String TRACEGEN_FILE_NAME = "tracegen.ix";

    private static final String FILTER_FILE_NAME = "icera_filter.tsf";

    private static final String MODEM_LOGGING_PORT = "32768";

    public Process mGenieProc = null;

    private boolean hasNewFilter;

    private String iosFilename = MODEM_DEBUG_DIRECTORY + File.separator + IOS_FILE_NAME;;

    private String filterFilename = MODEM_DEBUG_DIRECTORY + File.separator + FILTER_FILE_NAME;

    private long mLoggingSize;

    protected int mNumOfLogSegments;

    private boolean noLimitLogging;
    
    private boolean mDeferred;
    
    private int mDeferredTimeout;
    
    private int mDeferredWatermark;

    public ModemLogger(Context ctx) {

        super(ctx);

        noLimitLogging = mPrefs.getNoLimit();
        mLoggingSize = mPrefs.getLoggingSize();
        mDeferred = mPrefs.isDeferredLoggingEnabled();
        mDeferredTimeout = mPrefs.getDeferredTimeoutPos();
        mDeferredWatermark = mPrefs.getDeferredWatermarkPos();
        
        if (noLimitLogging) {
            long freeSpace;

            if (onSDCard)
                freeSpace = Environment.getExternalStorageDirectory().getFreeSpace();
            else
                freeSpace = Environment.getDataDirectory().getFreeSpace();

            mLoggingSize = (int)(((freeSpace - LogUtil.FULL_COREDUMP_FILE_SIZE * 3 - SaveLogs.RESERVED_SPACE_SIZE * 1024 * 1024) / (1024 * 1024)) / SaveLogs.DEFAULT_IOM_SIZE)
                    * SaveLogs.DEFAULT_IOM_SIZE;
            mNumOfLogSegments = (int)(mLoggingSize / SaveLogs.DEFAULT_IOM_SIZE);

        } else
            mNumOfLogSegments = 5; // TODO: allow user to customize?

        hasNewFilter = new File(filterFilename).exists();

        installGenieTools();
    }

    /**
     * Copy modify filter tool into /data/data/icera.DebugAgent/app_tools
     * directory
     * 
     * @param bugReportService TODO
     */

    void installGenieTools() {
        InputStream in = null;
        OutputStream out = null;
        int size = 0;

        // Create storage directory for tools
        File toolsDir = mContext.getDir(GENIE_TOOLS_DIRECTORY, Context.MODE_WORLD_READABLE
                | Context.MODE_WORLD_WRITEABLE);

        try {
            in = mContext.getResources().getAssets().open(GENIE_IOS_TOOL);
            size = in.available();
            // Read the entire resource into a local byte buffer
            byte[] buffer = new byte[size];
            in.read(buffer);
            in.close();

            out = new FileOutputStream(toolsDir.getAbsolutePath() + "/" + GENIE_IOS_TOOL);

            out.write(buffer);
            out.close();
            Runtime.getRuntime().exec(
                    "chmod 777 data/data/icera.DebugAgent/app_tools/genie-ios-arm");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    void uninstallGenieTools() {
        // TODO
    }

    boolean isGenieToolInstalled() {
        // TODO
        return true;
    }

    /*
     * Retrieve tracegen.ix from the modem
     */
    public void getTracegen() {

        File port = new File(MODEM_LOGGING_DEV);
        // File outputTracegen = new
        // File("/data/data/icera.DebugAgent/files/tracegen.ix");
        File outputTracegen = new File(mLoggingDir + File.separator + TRACEGEN_FILE_NAME);
        String cmd = "icera_log_serial_arm -e" + outputTracegen.getAbsolutePath()
                + " -d/dev/log_modem";
        
        if (port.exists()
                && (!outputTracegen.exists() || (outputTracegen.exists() && (outputTracegen
                        .length() < 500000)))) {
            // stopAndClean();
            try {
                Runtime.getRuntime().exec(cmd);
                Thread.sleep(2000);
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            } catch (InterruptedException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }
    }

    /*
     * For modem logger, we need to make sure there is no other instance
     * running. and if there is filter file, we need to generate the ios file
     * first. and always get tracegen first
     * @see icera.DebugAgent.Logger#beforeStart()
     */
    @Override
    protected boolean beforeStart() {

        // Call super to initialize the logging directory
        if (!super.beforeStart())
            return false;

        // TODO: check if ios and logging tools are installed?

        if (checkLoggingProcess()) {
            Log.e(TAG, "icera_log_serial_arm already running!");
            mLoggingDir.delete();

            // TODO: kill the running processes?

            return false;
        }

        if (!(new File(MODEM_LOGGING_DEV).exists())) {
            Log.e(TAG, "Modem logging device not found!");
            mLoggingDir.delete();
            return false;
        }

        getTracegen();

        ArrayList<String> progs = new ArrayList<String>();

        if (hasNewFilter) {
            progs.add("/data/data/icera.DebugAgent/app_tools/" + GENIE_IOS_TOOL);
            progs.add(iosFilename);
            //progs.add(mContext.getFilesDir().getAbsolutePath() + File.separator + TRACEGEN_FILE_NAME);
            progs.add(new File(mLoggingDir + File.separator + TRACEGEN_FILE_NAME).getAbsolutePath());
            progs.add(filterFilename);

            try {
                mGenieProc = Runtime.getRuntime().exec(progs.toArray(new String[0]));
            } catch (IOException e) {
                Log.e(TAG, "Failed to generate IOS file!");
                mGenieProc = null;
                e.printStackTrace();
                return false;
            }
        }

        return true;
    }

    @Override
    protected void prepareProgram() {

        mProgram.clear();
        mProgram.add(GENIE_LOGGING_TOOL);
        mProgram.add("-b");
        mProgram.add("-d" + MODEM_LOGGING_DEV);
        mProgram.add("-p" + MODEM_LOGGING_PORT);
        mProgram.add("-f");
        if (hasNewFilter)
            mProgram.add("-s" + iosFilename);
        if (!mPrefs.isBeanieVersionOld()){
            if (mDeferred){
                mProgram.add("-r1" + "," + mDeferredWatermark + "," + mDeferredTimeout);
            }
            else{
                mProgram.add("-r0");
            }
        }
        mProgram.add("-a" + mLoggingDir + "," + mLoggingSize + "," + mLoggingSize
                / mNumOfLogSegments);
    }

    public boolean checkLoggingProcess() {

        BufferedReader br = null;
        boolean proc_present = false;
        String line = "";

        try {
            Process p = Runtime.getRuntime().exec(new String[] {
                "ps"
            });
            br = new BufferedReader(new InputStreamReader(p.getInputStream()));

            while ((line = br.readLine()) != null) {

                if (line.contains("icera_log_serial_arm"))
                    proc_present = true;
            }
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        return proc_present;
    }

    @Override
    protected void updateLogList() {
        FilenameFilter filter = new FilenameFilter() {

            @Override
            public boolean accept(File dir, String filename) {

                if (filename.endsWith(".iom") || filename.contentEquals("daemon.log")
                        || filename.contentEquals("comment.txt")
                        || filename.contentEquals("tracegen.ix"))
                    return true;
                else
                    return false;
            }

        };

        for (File file : mLoggingDir.listFiles(filter)) {
            this.addToLogList(file);
        }
        /*
         * File tracegen = new
         * File("/data/data/icera.DebugAgent/files/tracegen.ix"); if
         * (!tracegen.exists()) getTracegen(); addToLogList(tracegen);
         */
    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "MDM";
    }
}
