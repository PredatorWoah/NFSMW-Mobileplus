// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;
import android.widget.Toast;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/**
 * "Update available" for the launcher. The public repository holds only a
 * small manifest (speedbreaker/update/latest.json: package, version, notes and
 * where to get it); the APK itself, which carries the recompiled game, lives in
 * the owner's private builds repository and is never published. Download opens
 * that release page in the browser (signed in to GitHub there), and the APK,
 * signed with the same key, installs over this one keeping the saves.
 *
 * Checked when the launcher opens, at most every 6 hours, and on demand from
 * its "Check for updates" button. A manifest for another package (the .test
 * probe, the .plus build) is ignored.
 */
final class UpdateCheck {
    private static final String TAG = "SpeedBreakerOne";
    static final String MANIFEST = "https://raw.githubusercontent.com/PredatorWoah/NFSMW-Mobileplus/main/speedbreaker/update/latest.json";
    private static final long INTERVAL_MS = 6L * 3600 * 1000;

    private UpdateCheck() { }

    static void check(Activity activity, boolean manual) {
        SharedPreferences prefs = activity.getSharedPreferences("update_check", Activity.MODE_PRIVATE);
        long now = System.currentTimeMillis();
        if (!manual && now - prefs.getLong("last", 0) < INTERVAL_MS) return;
        prefs.edit().putLong("last", now).apply();
        new Thread(() -> {
            String error = null;
            JSONObject latest = null;
            try {
                latest = new JSONObject(fetch(MANIFEST));
            } catch (Exception e) {
                error = e.getMessage();
                Log.w(TAG, "[update] check failed: " + e);
            }
            final JSONObject found = latest;
            final String failed = error;
            activity.runOnUiThread(() -> {
                if (activity.isFinishing() || activity.isDestroyed()) return;
                if (found == null) {
                    if (manual) Toast.makeText(activity, "Couldn't check for updates: " + failed, Toast.LENGTH_LONG).show();
                    return;
                }
                show(activity, found, manual);
            });
        }, "SB update check").start();
    }

    private static void show(Activity activity, JSONObject latest, boolean manual) {
        boolean ours = BuildConfig.APPLICATION_ID.equals(latest.optString("applicationId"));
        int code = latest.optInt("versionCode", 0);
        if (!ours || code <= BuildConfig.VERSION_CODE) {
            if (manual) Toast.makeText(activity, ours ? "Up to date (" + BuildConfig.VERSION_NAME + ")"
                    : "No updates are published for this build (" + BuildConfig.APPLICATION_ID + ")", Toast.LENGTH_LONG).show();
            return;
        }
        String url = latest.optString("url");
        new AlertDialog.Builder(activity)
                .setTitle("Update available: " + latest.optString("versionName") + " (you have " + BuildConfig.VERSION_NAME + ")")
                .setMessage(latest.optString("notes") + "\n\nDownload opens the private release page in your browser (sign in to GitHub there). "
                        + "Install the APK over this app: your saves and settings stay.")
                .setNegativeButton("Later", null)
                .setPositiveButton("Download", (d, w) -> {
                    try { activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))); }
                    catch (Exception e) { Toast.makeText(activity, "No browser to open " + url, Toast.LENGTH_LONG).show(); }
                })
                .show();
    }

    private static String fetch(String address) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(address).openConnection();
        c.setConnectTimeout(8000);
        c.setReadTimeout(8000);
        c.setUseCaches(false);
        c.setRequestProperty("Cache-Control", "no-cache");
        try {
            if (c.getResponseCode() != 200) throw new Exception("HTTP " + c.getResponseCode());
            try (InputStream in = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[8192];
                for (int n; (n = in.read(buf)) > 0 && out.size() < 64 * 1024; ) out.write(buf, 0, n);
                return out.toString(StandardCharsets.UTF_8.name());
            }
        } finally {
            c.disconnect();
        }
    }
}
