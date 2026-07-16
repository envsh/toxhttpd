package io.fedlet.mobutil;

import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import org.json.JSONArray;
import org.qtproject.qt.android.bindings.QtActivity;

public class ShareActivity extends QtActivity {
    private static native void onShareIntentReceived(
        String action, String mimeType, String text, String urisJson);

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        handleIntent(getIntent());
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent);
    }

    private void handleIntent(Intent intent) {
        String action = intent.getAction();
        if (action == null) return;

        if (!Intent.ACTION_SEND.equals(action)
                && !Intent.ACTION_SEND_MULTIPLE.equals(action)) {
            return;
        }

        String type = intent.getType();
        String text = intent.getStringExtra(Intent.EXTRA_TEXT);
        JSONArray uris = new JSONArray();

        ClipData clipData = intent.getClipData();
        if (clipData != null) {
            for (int i = 0; i < clipData.getItemCount(); i++) {
                Uri uri = clipData.getItemAt(i).getUri();
                if (uri != null) {
                    uris.put(uri.toString());
                }
            }
        }

        onShareIntentReceived(action, type, text, uris.toString());
    }
}
