// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

import android.content.Context;
import android.os.Environment;
import android.util.Log;

public abstract class LogUtil {

    private static final String TAG = "LogUtil";

    public static final SimpleDateFormat LOG_DATE_FORMAT = new SimpleDateFormat("MMM-dd_HH-mm-ss",
            Locale.US);

    public static final String LOG_STORE_HOME = "BugReport_Logs";

    public static final String DEFAULT_STORE_ROOT = "/data/rfs/data/debug" + File.separator
            + LOG_STORE_HOME;

    public static final String DEFAULT_LOGGING_DIRECTORY_SD = ".LOGGING_DONT_REMOVE";

    public static final long FULL_COREDUMP_FILE_SIZE = 32 * 1024 * 1024L;

    protected Context mContext;

    protected Prefs mPrefs;

    protected File mLoggingDir = null;

    protected String mLoggerName = null;

    private ArrayList<File> mLogList = new ArrayList<File>();

    protected long mTotalLogSize = 0;

    boolean onSDCard;

    /*
     * The program we will run to generate the logs
     */
    protected List<String> mProgram = new ArrayList<String>();

    public LogUtil(Context ctx) {

        super();

        mContext = ctx;
        mPrefs = new Prefs(mContext);

        // Mandate to use SD if available
        onSDCard = isExternStorageUsable(); // = mPrefs.isLoggingOnSDCard();
    }

    /*
     * Preconditions for the logging program
     */
    protected boolean beforeStart() {

        boolean sdUsable = isExternStorageUsable();

        if (onSDCard && !sdUsable)
            return false;

        // Default logging folders
        File loggingRoot = mContext.getFilesDir();

        // Change to SD if available
        if (sdUsable) {
            String storeRoot = Environment.getExternalStorageDirectory().getAbsolutePath()
                    + File.separator + LOG_STORE_HOME;
            if (onSDCard)
                loggingRoot = new File(Environment.getExternalStorageDirectory().getAbsolutePath()/* storeRoot */
                        + File.separator + DEFAULT_LOGGING_DIRECTORY_SD);
        }

        if (!loggingRoot.exists())
            loggingRoot.mkdirs();

        String loggingDirName = loggingRoot.getAbsolutePath() + File.separator
                + LOG_DATE_FORMAT.format(new Date());
        setLoggerName();
        if (mLoggerName != null)
            loggingDirName += "_" + mLoggerName;

        mLoggingDir = new File(loggingDirName/* + "_LOGGING" */);
        mLoggingDir.mkdir();

        return true;
    }

    /*
     * Construct the logging program
     */
    protected abstract void prepareProgram();

    /*
     * Collect the log files according to the logger type
     */
    protected abstract void updateLogList();

    /*
     * Run the logger program to generate the log files
     */
    protected abstract boolean start();

    protected void deleteLogs() {

        for (File file : mLogList) {
            if (file.isDirectory()) {
                for (File f : file.listFiles()) {
                    if (!f.delete())
                        Log.d(TAG, "<" + (mLoggerName != null ? mLoggerName : "") + ">"
                                + "Failed to delete log file: " + f.getName());
                }
            }
            if (!file.delete())
                Log.d(TAG, "<" + (mLoggerName != null ? mLoggerName : "") + ">"
                        + "Failed to delete log file: " + file.getName());
        }

        mLogList.clear();
        if (!mLoggingDir.delete())
            Log.d(TAG, "Failed to delete logging directory: " + mLoggingDir.getAbsolutePath());
    }

    private void copyFile(File destDir, File file, StoreProgressListener progListener) {
        int read;
        long total = 0;
        byte[] buffer = new byte[1024];
        InputStream from = null;
        OutputStream to = null;

        try {
            from = new BufferedInputStream(new FileInputStream(file));
            to = new BufferedOutputStream(new FileOutputStream(new File(destDir + File.separator
                    + file.getName())));

            while ((read = from.read(buffer)) != -1) {
                total += read;
                to.write(buffer, 0, read);
            }
            if (progListener != null)
                progListener.storeProgressUpdate((int)((total * 100) / mTotalLogSize));

            // from.close();
            // to.close();
        } catch (FileNotFoundException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        } catch (IOException e) {
            if (from != null)
                try {
                    from.close();
                    from = null;
                } catch (IOException e1) {
                    e1.printStackTrace();
                }
            if (to != null)
                try {
                    to.close();
                    to = null;
                } catch (IOException e1) {
                    e1.printStackTrace();
                }
            e.printStackTrace();
        } finally {
            if (from != null)
                try {
                    from.close();
                    from = null;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            if (to != null)
                try {
                    to.close();
                    to = null;
                } catch (IOException e) {
                    e.printStackTrace();
                }
        }
    }

    /*
     * Store the log list to the destination folder, and delete the logging
     * files. Make sure the logging is stopped before calling this
     */
    public void store(File destDir, boolean onSD, StoreProgressListener progListener) {

        long total = 0;

        if (onSD ^ onSDCard) {
            for (File file : mLogList) {
                if (file.isDirectory()) {
                    for (File f : file.listFiles()) {
                        File newDestDir = new File(destDir.getAbsolutePath() + File.separator
                                + file.getName());
                        newDestDir.mkdirs();
                        copyFile(newDestDir, f, progListener);
                    }
                } else if (file.isFile()) {
                    copyFile(destDir, file, progListener);
                }
            }
        } else {
            for (File file : mLogList) {
                if (!file.renameTo(new File(destDir + File.separator + file.getName()))) {
                    Log.e(TAG, "<" + (mLoggerName != null ? mLoggerName : "") + ">"
                            + "Failed to store log file: " + file.getName());
                }
                total += file.length();
                if (progListener != null && mTotalLogSize != 0)
                    progListener.storeProgressUpdate((int)((total * 100) / mTotalLogSize));
            }
        }

        deleteLogs();
    }

    protected void calculateLogSize() {

        mTotalLogSize = 0;
        for (File file : mLogList) {
            if (file.isDirectory()) {
                for (File f : file.listFiles()) {
                    mTotalLogSize += f.length();
                }
            } else if (file.isFile())
                mTotalLogSize += file.length();
        }
    }

    public final long getLogSize() {

        return mTotalLogSize;
    }

    protected final void addToLogList(File file) {
        mLogList.add(file);
    }

    protected final void removeFromLogList(File file) {
        mLogList.remove(file);
    }

    protected final void clearLogList() {
        mLogList.clear();
    }

    public static boolean isExternStorageUsable() {

        String state = Environment.getExternalStorageState();

        if (!state.contentEquals(Environment.MEDIA_MOUNTED)
                || !Environment.getExternalStorageDirectory().canWrite()) {
            Log.e(TAG, "SD card not mounted or not writable");
            return false;
        }

        return true;
    }

    public interface StoreProgressListener {
        public void storeProgressUpdate(int progress);
    }

    protected abstract void setLoggerName();
}
