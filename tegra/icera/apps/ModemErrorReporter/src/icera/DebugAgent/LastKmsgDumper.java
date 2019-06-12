// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.io.BufferedReader;
import java.io.FileReader;
import android.content.Context;

/**
 * @author jeffreyz
 */
public class LastKmsgDumper extends LogDumper {

    private static final String LASTKMSG_FILE_NAME = "last_kmsg.txt";

    public LastKmsgDumper(Context ctx) {

        super(ctx);

    }

    /**
     * Check if a kernel crash log is present in the file system
     * 
     * @param bugReportService TODO
     */
    public boolean checkKernelState() {
        boolean crash = false;

        try {
            String lastKmsgFile = mContext.getString(R.string.last_kmsg_filename);
            BufferedReader kmsgContent = new BufferedReader(new FileReader(lastKmsgFile));
            if (kmsgContent != null) {
                String str;

                while ((str = kmsgContent.readLine()) != null) {
                    if (str.contains("Internal error")) {
                        crash = true;
                        kmsgContent.close();
                        break;
                    }
                }
            }
        } catch (java.io.FileNotFoundException e) {
            // that's OK, we probably haven't created it yet
            e.printStackTrace();
        } catch (Throwable t) {

        }

        return crash;
    }

    @Override
    protected void prepareProgram() {

        mProgram.clear();
        mProgram.add("cat");
        mProgram.add("/proc/last_kmsg");
    }

    @Override
    protected void updateLogList() {

    }

    @Override
    protected void setLogFileName() {

        mLogFileName = LASTKMSG_FILE_NAME;
    }

    @Override
    protected void setLoggerName() {
        mLoggerName = "LASTKMSG";
    }
}
