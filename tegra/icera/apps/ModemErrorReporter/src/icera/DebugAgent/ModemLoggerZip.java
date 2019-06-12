// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.File;
import java.io.FilenameFilter;

import android.content.Context;

/*A subclass of ModemLogger that implement modem logging.
 * Define by Jacky Zhuo on Mar 9, 2013
 */
public class ModemLoggerZip extends ModemLogger {
    private ZipThread zipLogging = null;

    public ModemLoggerZip(Context ctx) {
        super(ctx);
        zipLogging = null;
    }

    @Override
    public boolean start() {
        if (!super.start())
            return false;
        if (zipLogging == null) {
            zipLogging = new ZipThread(mLoggingDir.getAbsolutePath(), mNumOfLogSegments);
            zipLogging.start();
        }
        return true;
    }

    @Override
    protected void killLogProc() {
        super.killLogProc();
        if (zipLogging != null) {
            zipLogging.tryToStop();
        }
    }

    @Override
    public void stop() {
        killLogProc();
        preCalculateLogSize();
    }

    @Override
    public void stopAndClean() {

        super.killLogProc();
        if (zipLogging != null) {
            zipLogging.Stop();
            zipLogging = null;
        }
        updateLogList();
        calculateLogSize();
        deleteLogs();
    }

    @Override
    public void store(File destDir, boolean onSD, StoreProgressListener spl) {
        if (zipLogging != null) {
            try {
                zipLogging.join(600000);
                zipLogging = null;
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        updateLogList();
        calculateLogSize();
        super.store(destDir, onSD, spl);
    }

    protected void preCalculateLogSize() {

        mTotalLogSize = getLogList(false);
    }

    @Override
    protected void updateLogList() {

        getLogList(true);
    }

    protected int getLogList(boolean addOrGetLen) {

        FilenameFilter filter = new FilenameFilter() {

            @Override
            public boolean accept(File dir, String filename) {

                if (filename.endsWith(".zip") || filename.endsWith(".ZIP")
                        || filename.contentEquals("daemon.log")
                        || filename.contentEquals("comment.txt")
                        || filename.contentEquals("tracegen.ix") || filename.endsWith(".iom"))
                    return true;
                else
                    return false;
            }
        };
        int totalLen = 0;
        if (addOrGetLen) {
            for (File file : mLoggingDir.listFiles(filter)) {
                this.addToLogList(file);
            }
        } else {
            for (File file : mLoggingDir.listFiles(filter)) {
                totalLen += file.length();
            }
        }

        return totalLen;
    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "MDMZIP";
    }

}
