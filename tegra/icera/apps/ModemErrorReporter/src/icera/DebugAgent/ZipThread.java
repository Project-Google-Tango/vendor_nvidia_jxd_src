// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.File;
import java.io.FilenameFilter;

/* A Thread monitor the log file and compress them.
 * Author: Jacky Zhuo
 */

public class ZipThread extends Thread {

    private String originalFile = "";

    private boolean zipThreadRunning = true;

    private boolean mKilled = true;

    private String beanieLogDirTMP;

    private int mNumOfmNumOfLogSegments;

    public ZipThread(String logDir, int numOfFile) {
        // super();
        beanieLogDirTMP = logDir;
        mNumOfmNumOfLogSegments = numOfFile;
    }

    public ZipThread() {
        beanieLogDirTMP = "";
        mNumOfmNumOfLogSegments = 0;
    }

    @Override
    public void run() {
        monitorLogFolder();
        copyRemainedFiles();

    }

    public void tryToStop() {
        zipThreadRunning = false;
    }

    public void Stop() {
        mKilled = false;
        tryToStop();
    }

    private File[] getFilterFile(String dir, String myFilter) {
        java.io.File file = new File(dir);
        return file.listFiles(new MyFilter(myFilter));
    }

    /*
     * Monitor the log files. Once a log file was closed by logging tool, zip
     * it. Then delete it.
     */

    private Boolean monitorLogFolder() {
        File[] file_list;

        // this.setPriority(MAX_PRIORITY);
        this.setPriority(NORM_PRIORITY);
        while (zipThreadRunning) {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

            if (beanieLogDirTMP == null || mNumOfmNumOfLogSegments < 3) {
                continue;
            }

            // list all the files with suffix ".iom"
            file_list = getFilterFile(beanieLogDirTMP, ".iom");
            if (file_list == null) {
                continue;
            }
            if (file_list.length > 1) {
                // Sometimes the last file is not older than the new file that
                // is being created by beanie tool. So find it here.
                File oldest = findOldestFile(file_list, findSmallestFileNum(file_list));
                try {
                    // If the file is the original *_000.iom, Mark it with the
                    // suffix .ZIP, the file will not be removed.
                    if (oldest.getName().endsWith("_000.iom")) {
                        originalFile = oldest.getName();
                        LogZipper.zipFile(oldest.getAbsolutePath(), beanieLogDirTMP
                                + File.separator + oldest.getName() + ".ZIP");
                    } else {
                        LogZipper.zipFile(oldest.getAbsolutePath(), beanieLogDirTMP
                                + File.separator + oldest.getName() + ".zip");
                    }
                    if (!oldest.delete()) {
                        // stop monitoring
                        break;
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }
                // If the *zip file num is more than the num user setting,
                // delete the oldest.
                checkDeleteLog(beanieLogDirTMP, ".zip");
            }
        }
        return true;
    }

    /*
     * Compress or copy the all of remained files when stop logging.
     */
    private boolean copyRemainedFiles() {
        if (!mKilled)
            return true;
        File[] file_list;

        while (true) {
            file_list = getFilterFile(beanieLogDirTMP, ".iom");
            File oldest = findOldestFile(file_list);
            if (0 == file_list.length) {
                break;
            }
            try {
                LogZipper.zipFile(oldest.getAbsolutePath(), beanieLogDirTMP + File.separator
                        + oldest.getName() + ".zip");
                if (!oldest.delete()) {
                    break;
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            if (mNumOfmNumOfLogSegments > 2)
                checkDeleteLog(beanieLogDirTMP, ".zip");
        }

        return new File(beanieLogDirTMP + File.separator + originalFile + ".ZIP")
                .renameTo(new File(beanieLogDirTMP + File.separator + originalFile + ".zip"));

    }

    /*
     * If the files is more than mNumOfmNumOfLogSegments, delete it.
     */
    private boolean checkDeleteLog(String zipLogPath, String filter) {
        File[] file_list = getFilterFile(zipLogPath, filter);
        while (file_list.length > mNumOfmNumOfLogSegments - 1) {
            if (!findOldestFile(file_list).delete())
                return false;
            file_list = getFilterFile(zipLogPath, filter);
        }
        return true;
    }

    /*
     * find the oldest log file. param file_list: a array saved file. param
     * filter: if the oldest number is the filter, discard it.
     */
    private File findOldestFile(File[] file_list, int filter) {
        for (int i = 0; i < file_list.length; i++) {
            if (i == filter) {
                continue;
            }
            int j = 0;
            for (j = i + 1; j < file_list.length; j++) {
                if (j == filter) {
                    continue;
                }
                if (file_list[i].lastModified() > file_list[j].lastModified())
                    break;
            }
            if (j >= file_list.length) {
                return file_list[i];
            }
        }
        return null;
    }

    private File findOldestFile(File[] file_list) {
        for (int i = 0; i < file_list.length; i++) {
            int j = 0;
            for (j = i + 1; j < file_list.length; j++) {
                if (file_list[i].lastModified() > file_list[j].lastModified())
                    break;
            }
            if (j >= file_list.length) {
                return file_list[i];
            }
        }
        return null;
    }

    /*
     * find the oldest log file. param file_list: a array saved file. Return the
     * smallest file number
     */
    private int findSmallestFileNum(File[] file_list) {
        for (int i = 0; i < file_list.length; i++) {
            int j = 0;
            for (j = i + 1; j < file_list.length; j++) {
                if (file_list[i].length() > file_list[j].length())
                    break;
            }
            if (j >= file_list.length)
                return i;
        }
        return 0;
    }

    public class MyFilter implements FilenameFilter {
        private String type;

        public MyFilter(String type) {
            this.type = type;
        }

        public boolean accept(File dir, String name) {
            return name.endsWith(type);
        }
    }
}
