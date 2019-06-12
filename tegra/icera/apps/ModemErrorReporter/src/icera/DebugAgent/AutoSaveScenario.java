// Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import java.util.HashMap;

public class AutoSaveScenario {

    int id;
    String keyword;
    int delay;
    String dirSuffix;
    String mCancelKeyword;
    boolean mCancelable = false;
    private boolean mEnabled = false;
    private static HashMap<String, AutoSaveScenario> mmap = new HashMap();
    
    public AutoSaveScenario(int scId, String scKeyword, int scDelay, String scSuffix) {

            id = scId;
            keyword = scKeyword;
            delay = scDelay;
            dirSuffix = scSuffix;
            mmap.put(scKeyword, this);
    }

    public AutoSaveScenario(int scId, String scKeyword, int scDelay, String scSuffix, String scCancelKeyword, boolean scCancelable) {
        this(scId, scKeyword, scDelay, scSuffix);
        mCancelKeyword = scCancelKeyword;
        mCancelable = scCancelable;
    }

    public static void setEnable(String which, boolean enable){
        AutoSaveScenario obj = mmap.get(which);
        if ( obj != null)
            obj.setEnale(enable);
    }

    public void setEnale(boolean enable){
        mEnabled = enable;
    }

    public void setCancelable(boolean cancelable) {
        mCancelable = cancelable;
    }

    public boolean isCancelable() {
        return mCancelable;
    }

    public boolean occurrs(String logLine) {

        if (!mEnabled)
            return false;
        if (logLine.contains(keyword))
            return true;

        return false;
    }
    /*
     * Check if the cancelKeyword occurrs in the given log line.
     */
    public boolean cancelingOccurrs(String logLine) {

        if (!mEnabled || !mCancelable)
            return false;

        if ((mCancelKeyword != null) && logLine.matches(mCancelKeyword))
            return true;

        return false;
    }
}
