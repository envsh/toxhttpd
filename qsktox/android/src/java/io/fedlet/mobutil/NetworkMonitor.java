package io.fedlet.mobutil;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

public class NetworkMonitor {
    private static native void onNetworkChanged(boolean isConnected, String networkType);
    private static BroadcastReceiver receiver;

    public static void startMonitoring(Context ctx) {
        if (receiver != null) return;
        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                ConnectivityManager cm = (ConnectivityManager)
                    context.getSystemService(Context.CONNECTIVITY_SERVICE);
                NetworkInfo info = cm.getActiveNetworkInfo();
                boolean connected = info != null && info.isConnected();
                String type = "Unknown";
                if (connected && info != null) {
                    int t = info.getType();
                    if (t == ConnectivityManager.TYPE_WIFI) {
                        type = "WiFi";
                    } else if (t == ConnectivityManager.TYPE_MOBILE) {
                        type = "Mobile (" + info.getSubtypeName() + ")";
                    } else {
                        type = info.getTypeName();
                    }
                }
                onNetworkChanged(connected, type);
            }
        };
        IntentFilter filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
        ctx.registerReceiver(receiver, filter);
    }

    public static void stopMonitoring(Context ctx) {
        if (receiver != null) {
            ctx.unregisterReceiver(receiver);
            receiver = null;
        }
    }
}
