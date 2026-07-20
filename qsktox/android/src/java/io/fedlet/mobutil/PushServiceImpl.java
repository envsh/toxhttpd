package io.fedlet.mobutil;

import org.unifiedpush.android.connector.PushService;
import org.unifiedpush.android.connector.data.PushEndpoint;
import org.unifiedpush.android.connector.data.PushMessage;
import org.unifiedpush.android.connector.FailedReason;

public class PushServiceImpl extends PushService {
    static { System.loadLibrary("qsktox_arm64-v8a"); }

    private static native void onNewEndpointNative(String endpoint, String instance);
    private static native void onMessageNative(byte[] message, String instance);
    private static native void onRegistrationFailedNative(String reason, String instance);
    private static native void onUnregisteredNative(String instance);

    @Override
    public void onNewEndpoint(PushEndpoint endpoint, String instance) {
        onNewEndpointNative(endpoint.getUrl(), instance);
    }

    @Override
    public void onMessage(PushMessage message, String instance) {
        onMessageNative(message.getContent(), instance);
    }

    @Override
    public void onRegistrationFailed(FailedReason reason, String instance) {
        onRegistrationFailedNative(reason.name(), instance);
    }

    @Override
    public void onUnregistered(String instance) {
        onUnregisteredNative(instance);
    }
}
