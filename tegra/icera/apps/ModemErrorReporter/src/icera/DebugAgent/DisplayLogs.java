// Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

package icera.DebugAgent;

import icera.DebugAgent.BugReportService.BugReportServiceBinder;

import java.io.*;
import java.util.*;

import android.app.*;
import android.content.*;
import android.os.Bundle;
import android.os.Environment;
import android.os.IBinder;
import android.view.View;
import android.widget.*;

public class DisplayLogs extends ListActivity {
    /** Called when the activity is first created. */

    private File file;

    private List<String> myList;

    // Binding to BugReportService
    BugReportService mService = null;

    LoggerManager mLoggerMgr = null;

    boolean mBound = false;

    public void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        myList = new ArrayList<String>();

        String root_sd = Environment.getExternalStorageDirectory().toString();
        file = new File(root_sd + "/BugReport_Logs");
        file.mkdirs();

        if (!file.exists()) {
            file = new File("/data/rfs/data/debug/BugReport_Logs");
            file.mkdirs();
        }

        File[] list = dirListByDescendingDate(file);

        for (int i = 0; i < list.length; i++) {
            myList.add(list[i].getName());
        }
        setListAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, myList));

        // Bind to service
        Intent intent = new Intent(this, BugReportService.class);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);

    }

    protected void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);

        final File temp_file = new File(file, myList.get(position));

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage("Are you sure you want to delete " + temp_file.getName() + " ?")
                .setCancelable(false)
                .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        deleteDirectory(temp_file);
                        DisplayLogs.this.finish();
                    }
                }).setNegativeButton("No", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        dialog.cancel();
                    }
                });
        builder.show();

    }

    @Override
    public void onBackPressed() {
        DisplayLogs.this.finish();
    }

    public boolean deleteDirectory(File path) {

        int directory_length = 0;
        if (path.exists()) {
            File[] files = path.listFiles();
            if (files == null) {
                return true;
            }
            for (int i = 0; i < files.length; i++) {
                if (files[i].isDirectory())
                    deleteDirectory(files[i]);
                else {
                    directory_length += files[i].length() / 1048576;
                    files[i].delete();
                }
            }
            Toast.makeText(this, "Deleted " + directory_length + "Mo", Toast.LENGTH_LONG).show();
         //   Toast.makeText(this, "Note: At least 600M space needed for max space modem logging",
           //         Toast.LENGTH_LONG).show();

            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
            }
            // if (Environment.getExternalStorageDirectory().getFreeSpace() >
            // BugReportService.LOGGING_STOP_THRESHHOLD) {
            // mService.notifyMessage(NotificationType.NOTIFY_SDCARD_OK);
            // try {
            // Thread.sleep(1000);
            // } catch (InterruptedException e) {
            // // TODO Auto-generated catch block
            // e.printStackTrace();
            // }
            // mLoggerMgr.resumeLoggers();

            // }

        }
        return (path.delete());
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
        }
    }

    /** Defines callbacks for service binding, passed to bindService() */
    private ServiceConnection mConnection = new ServiceConnection() {

        public void onServiceConnected(ComponentName className, IBinder service) {
            // We've bound to LocalService, cast the IBinder and get
            // LocalService instance
            BugReportServiceBinder binder = (BugReportServiceBinder)service;
            mService = binder.getService();
            mLoggerMgr = mService.getLoggerManager();
            mBound = true;
        }

        public void onServiceDisconnected(ComponentName arg0) {
            mService = null;
            mLoggerMgr = null;
            mBound = false;
        }
    };

    @SuppressWarnings({
            "unchecked", "rawtypes"
    })
    public File[] dirListByDescendingDate(File folder) {
        if (!folder.isDirectory()) {
            return null;
        }
        File files[] = folder.listFiles();
        if (files.length != 0) {
            Arrays.sort(files, new Comparator() {
                public int compare(final Object o1, final Object o2) {
                    return new Long(((File)o2).lastModified()).compareTo(new Long(((File)o1)
                            .lastModified()));
                }
            });
        }
        return files;
    }
}
