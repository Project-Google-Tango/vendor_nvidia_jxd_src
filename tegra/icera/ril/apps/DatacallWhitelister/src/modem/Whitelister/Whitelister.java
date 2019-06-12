package modem.Whitelister;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import android.net.IConnectivityManager;
import android.net.ConnectivityManager;
import android.os.ServiceManager;
import android.os.RemoteException;
import android.util.Log;

public class Whitelister extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) 
    {   
        if (intent.getAction().equals("android.intent.action.BOOT_COMPLETED")) {
            try {
            IConnectivityManager connMgr =
                    IConnectivityManager.Stub.asInterface(ServiceManager.getService(Context.CONNECTIVITY_SERVICE));
        
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE,true);
                Log.e("Icera", "Whitelisting datacalls ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_MMS,true);
                Log.e("Icera", "Whitelisting MMS ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_SUPL,true);
                Log.e("Icera", "Whitelisting SUPL ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_DUN,true);
                Log.e("Icera", "Whitelisting DUN ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_HIPRI,true);
                Log.e("Icera", "Whitelisting HIPRI ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_FOTA,true);
                Log.e("Icera", "Whitelisting FOTA ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_IMS,true);
                Log.e("Icera", "Whitelisting IMS ok");
                connMgr.setDataDependency(ConnectivityManager.TYPE_MOBILE_CBS,true);
                Log.e("Icera", "Whitelisting CBS ok");
            }
            catch (RemoteException e) {
                Log.e("Icera", "Whitelising datacalls error:" + e);
            }
        }
    }
}
