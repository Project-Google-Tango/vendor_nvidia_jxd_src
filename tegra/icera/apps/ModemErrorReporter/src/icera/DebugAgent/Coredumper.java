// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.File;
import java.io.FilenameFilter;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import android.content.Context;
import android.util.Log;

public class Coredumper extends LogDumper {

    private static final String TAG = "Coredumper";

    public static final String MODEM_COREDUMP_DIR = "/data/rfs/data/debug";

    private static final String COREDUMP_LIST_FILE_NAME = "coredump_list.txt";

    private static final String FULL_COREDUMP_FILE_PREFIX = "coredump_";

    private static final String MINI_COREDUMP_FILE_PREFIX = "crash_dump";

    public List<String> mCoredumpList = new ArrayList<String>();

    public static final long FULL_COREDUMP_FILE_SIZE = 32 * 1024 * 1024L;

    private File mTempStoreDir;

    private String mRilLogPath;

    public Coredumper(Context ctx) {

        super(ctx);

        // Coredumps are always on flash
        onSDCard = false;
    }

    /**
     * Init list with existing coredump in directory
     * 
     * @param dir the coredump directory.
     */
    void initCoredumpList(String dir) {
        File coredumpDir = new File(dir);
        if (coredumpDir.exists()) {
            String[] listFiles = coredumpDir.list();
            if (listFiles != null) {
                for (int i = 0; i < listFiles.length; i++) {
                    if (listFiles[i].startsWith("coredump_")
                            || listFiles[i].startsWith("crash_dump")) {
                        mCoredumpList.add(listFiles[i]);
                    }
                }
            }
        }
    }

    void initCoredumpList() {
        initCoredumpList(MODEM_COREDUMP_DIR);
    }

    public List<String> getCoredumpList() {
        return mCoredumpList;
    }

    @Override
    protected void setLogFileName() {

        mLogFileName = COREDUMP_LIST_FILE_NAME;
    }

    @Override
    protected void prepareProgram() {

        mProgram.clear();
        // mProgram.add("ls");
        // mProgram.add("-l");
        // mProgram.add(MODEM_COREDUMP_DIR + File.separator + "*dump*");
        mProgram.add("cat");
        mProgram.add("<");
        mProgram.add("/dev/null");
    }

    /*
     * Need special treatment for coredump files.
     * @see icera.DebugAgent.LogSaver#updateLogList()
     */
    @Override
    protected void updateLogList() {

        File coredumpDir = new File(MODEM_COREDUMP_DIR + File.separator + "latest_coredump");
        String tempStoreDirName = LOG_DATE_FORMAT.format(new Date()) + "_DUMP";
        mTempStoreDir = new File(mContext.getFilesDir().getAbsolutePath() /* MODEM_COREDUMP_DIR */
                + File.separator + tempStoreDirName);
        if (!mTempStoreDir.mkdir())
            Log.e(TAG, "Failed to create coredump temporary store folder");

    
//      getAssertInfo(mTempStoreDir.getAbsolutePath());
        getAssertInfoFromFile(mRilLogPath, mTempStoreDir.getAbsolutePath());
        copyDirectory(coredumpDir,mTempStoreDir);
        
        File[] files = mTempStoreDir.listFiles();
        for (int i = 0; i < files.length; i++)
            addToLogList(files[i]);
        Log.d(TAG, "Found coredump directory: " + coredumpDir.getAbsolutePath());

        removeFromLogList(mLog);
    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "COREDMP";
    }

    @Override
    protected void deleteLogs() {

        super.deleteLogs();

        if (mTempStoreDir != null)
            mTempStoreDir.delete();
    }

    protected boolean getAssertInfoFromFile(String path, String logPath){
        BufferedReader reader = null;
        BufferedWriter buf = null;
        try {
            File file = new File(path);
            reader = new BufferedReader(new FileReader(path));
            if (file.length() > 500000)
            reader.skip(file.length() - 500000);
            String line;
            int count = 0;
            while ((line = reader.readLine()) != null) {
                if (line.contains("AT%DEBUG=3,0")) {
                    Log.d(TAG, "Modem crash:" + line);
                    buf = new BufferedWriter(new FileWriter(logPath + File.separator + "modem_crash.txt"));
                    buf.write(line + "\r\n");
                    count++;
                }
        //assume the crash info contains 160 line.
                else if (count > 0 && count < 161){
                    buf.write(line + "\r\n");
                    count++;
                }
        //try to get the latest "at%debug=3,0" keyword
                else if (count > 160){
                    buf.flush();
                    buf.close();
                    count = 0;
                }
            }
            if (buf != null){
                buf.flush();
                buf.close();
            }
            if (reader != null){
                reader.close();
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }

        return false;
    }

    public void setRilLogPath(String path){
        mRilLogPath = path;
    }

    /**
     * copyDirectory() copies the all directory from source to destination.
     *
     * @param source : path name of source directory
     * @param destination : path name of destination directory.
     */
    public void copyDirectory(File sourceLocation , File targetLocation) {

        if (sourceLocation.isDirectory()) {
            if (!targetLocation.exists()) {
                targetLocation.mkdir();
            }

            String[] children = sourceLocation.list();
            for (int i=0; i<children.length; i++) {
                copyDirectory(new File(sourceLocation, children[i]),
                        new File(targetLocation, children[i]));
            }
        } else {

            InputStream in;
            OutputStream out;
            try {
                in = new FileInputStream(sourceLocation);
                out = new FileOutputStream(targetLocation);

             // Copy the bits from instream to outstream
                byte[] buf = new byte[1024];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                in.close();
                out.close();
            } catch (FileNotFoundException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            } catch (IOException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
        }
    }
}
