package com.onenonlygit.speedbreakerone;
import android.os.Bundle;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.view.WindowManager;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import org.libsdl.app.SDLActivity;
public final class SpeedBreakerActivity extends SDLActivity {
    private static final int DISC_REQUEST = 7301;
    private volatile String pendingUri;
    private volatile String pendingName = "Disc image";
    private android.window.OnBackInvokedCallback probeBack;
    private TouchGamepadView touchControls;
    private boolean resumed;
    private boolean displayCallbackRegistered;
    private CheatsMenu cheatsMenu;
    private static native void nativeDiscResult(int fd, String name, String error);

    @Override protected void onCreate(Bundle state) {
        // Before SDL loads the native libraries, so loading problems are logged too.
        DownloadsLog.start(this, BuildConfig.PLATFORM_PROBE ? "probe" : "game");
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (!BuildConfig.PLATFORM_PROBE && getSharedPreferences("display_policy",MODE_PRIVATE).getBoolean("prefer60",true)) preferSixtyHertz();
        WindowManager.LayoutParams windowParams = getWindow().getAttributes();
        windowParams.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(windowParams);
        hideSystemBars();
        if (BuildConfig.PLATFORM_PROBE) {
            // The probe has no rendered game UI once its result dialog closes.
            // Use an explicit exit and Android's current Back dispatch, rather
            // than relying on a legacy SDL key event to finish a root task.
            android.widget.Button exit = new android.widget.Button(this);
            exit.setText("Exit diagnostics");
            exit.setOnClickListener(v -> finishProbe("Exit button"));
            android.widget.RelativeLayout.LayoutParams params = new android.widget.RelativeLayout.LayoutParams(
                    android.view.ViewGroup.LayoutParams.WRAP_CONTENT, android.view.ViewGroup.LayoutParams.WRAP_CONTENT);
            params.addRule(android.widget.RelativeLayout.ALIGN_PARENT_END);
            params.addRule(android.widget.RelativeLayout.ALIGN_PARENT_TOP);
            int margin = (int)(12 * getResources().getDisplayMetrics().density);
            params.setMargins(margin, margin, margin, margin);
            mLayout.addView(exit, params);
            probeBack = () -> finishProbe("Android Back callback");
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, probeBack);
        } else {
            touchControls = new TouchGamepadView(this);
            mLayout.addView(touchControls, new android.widget.RelativeLayout.LayoutParams(
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.MATCH_PARENT));
            // CHEATS, top right in the touch controls' bar (CheatsMenu).
            cheatsMenu = new CheatsMenu(this, touchControls);
            touchControls.setOnCheats(cheatsMenu::show);
        }
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        if (!BuildConfig.PLATFORM_PROBE && getSharedPreferences("display_policy",MODE_PRIVATE).getBoolean("prefer60",true)) preferSixtyHertz();
        if (touchControls != null) touchControls.resume();
    }

    @Override protected void onPause() {
        resumed = false;
        if (cheatsMenu != null) cheatsMenu.cancel();
        if (touchControls != null) touchControls.pause();
        super.onPause();
    }

    private void finishProbe(String source) {
        android.util.Log.i("SpeedBreakerOne", "[probe-ui] exit requested via " + source);
        finish();
    }

    // Called by the game-free probe, on its SDL thread. Keep the result text
    // scrollable and the dialog actions outside it on landscape phone screens.
    public void showProbeSummary(String summary) {
        if (!BuildConfig.PLATFORM_PROBE) return;
        runOnUiThread(() -> {
            android.widget.TextView text = new android.widget.TextView(this);
            text.setText(summary);
            int pad = (int)(16 * getResources().getDisplayMetrics().density);
            text.setPadding(pad, pad, pad, pad);
            android.widget.ScrollView scroll = new android.widget.ScrollView(this);
            scroll.addView(text);
            android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(this)
                    .setTitle("SpeedBreaker — platform diagnostics")
                    .setView(scroll)
                    .setPositiveButton("Close results", (d, which) -> {})
                    .setNegativeButton("Exit diagnostics", (d, which) -> finishProbe("summary Exit button"))
                    .create();
            dialog.setOnDismissListener(d -> android.util.Log.i("SpeedBreakerOne", "[probe-ui] summary dismissed"));
            dialog.show();
            // Leave room for the title and fixed action buttons even with large
            // text settings. The report itself scrolls inside this window.
            dialog.getWindow().setLayout((int)(getResources().getDisplayMetrics().widthPixels * 0.9),
                    (int)(getResources().getDisplayMetrics().heightPixels * 0.85));
        });
    }

    @Override public void onBackPressed() {
        if (BuildConfig.PLATFORM_PROBE) finishProbe("legacy Android Back");
        else super.onBackPressed();
    }

    // Called from the SDL thread. SAF grants access to this one document;
    // no storage-wide permission and no ISO copy are needed.
    public void pickDisc() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            try { startActivityForResult(intent, DISC_REQUEST); }
            catch (RuntimeException e) { nativeDiscResult(-1, "", "The Android file picker could not open."); }
        });
    }

    private int openDisc(Uri uri) throws java.io.IOException {
        try (ParcelFileDescriptor document = getContentResolver().openFileDescriptor(uri, "r")) {
            if (document == null) throw new java.io.IOException("No readable file descriptor.");
            pendingUri = uri.toString();
            pendingName = "Disc image";
            try (android.database.Cursor cursor = getContentResolver().query(uri,
                    new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) pendingName = cursor.getString(0);
            } catch (RuntimeException ignored) { }
            return document.detachFd(); // native code owns and closes it
        }
    }

    public int openSavedDisc() {
        String saved = getPreferences(MODE_PRIVATE).getString("disc_uri", null);
        if (saved == null) return -1;
        try { return openDisc(Uri.parse(saved)); }
        catch (java.io.IOException | RuntimeException e) { return -1; }
    }

    public String discName() { return pendingName; }

    public void rememberDisc() {
        String uri = pendingUri;
        if (uri == null) return;
        String previous = getPreferences(MODE_PRIVATE).getString("disc_uri", null);
        getPreferences(MODE_PRIVATE).edit().putString("disc_uri", uri).apply();
        if (previous != null && !previous.equals(uri)) {
            try { getContentResolver().releasePersistableUriPermission(Uri.parse(previous), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
            catch (RuntimeException ignored) { }
        }
    }

    public void abandonDisc() {
        String uri = pendingUri;
        pendingUri = null;
        if (uri == null) return;
        String saved = getPreferences(MODE_PRIVATE).getString("disc_uri", null);
        if (uri.equals(saved)) getPreferences(MODE_PRIVATE).edit().remove("disc_uri").apply();
        try { getContentResolver().releasePersistableUriPermission(Uri.parse(uri), Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (RuntimeException ignored) { }
    }

    // A new launch must start with fresh native globals and one Vulkan loader.
    // Home/background only pauses/stops the Activity and does not call this.
    @Override protected void onDestroy() {
        if (cheatsMenu != null) cheatsMenu.cancel();
        if (touchControls != null) touchControls.pause();
        if (probeBack != null) getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(probeBack);
        super.onDestroy();
        DownloadsLog.stop(() -> android.os.Process.killProcess(android.os.Process.myPid()));
    }

    // The game runs at 60 or 30 fps (Frame Rate: 60, 30, or Auto, which drops
    // to 30 when hot). On a panel left at 90 Hz, as a Vivo X300 Pro ships,
    // neither divides the refresh: frames alternate 2 and 1 refreshes and
    // motion judders with the fps locked. Ask for the panel's 60 Hz mode at
    // its current resolution (or the lowest multiple of 60), and tell the
    // system the content's rate so it does not switch back on its own.
    private void preferSixtyHertz() {
        android.view.Display display = getDisplay();
        if (display == null || mSurface == null) return;
        android.view.Display.Mode now = display.getMode(), best = null;
        // Once the panel refused 60 (checkRefresh), the lowest multiple of 60 above it: 120.
        float floor = sixtyRefused ? 61f : 0f;
        for (android.view.Display.Mode m : display.getSupportedModes()) {
            if (m.getPhysicalWidth() != now.getPhysicalWidth() || m.getPhysicalHeight() != now.getPhysicalHeight()) continue;
            if (m.getRefreshRate() < floor || !multipleOf60(m.getRefreshRate())) continue;
            if (best == null || m.getRefreshRate() < best.getRefreshRate()) best = m;
        }
        if (best == null) return;
        watchDisplay(display);
        getWindow().getDecorView().removeCallbacks(refreshCheck);
        getWindow().getDecorView().postDelayed(refreshCheck, 2000);
        sixtyHertz=best;
        applySixtyHertz();
        String chosen=String.format(java.util.Locale.ROOT,
                "[display] requested mode=%d %.1f Hz; current=%.1f Hz",best.getModeId(),best.getRefreshRate(),now.getRefreshRate());
        android.util.Log.i("SpeedBreakerOne",chosen);
        DownloadsLog.note(chosen);
        if(!displayCallbackRegistered) {
            displayCallbackRegistered=true;
            mSurface.getHolder().addCallback(new android.view.SurfaceHolder.Callback() {
                @Override public void surfaceCreated(android.view.SurfaceHolder h) { requestSurfaceRate(h); }
                @Override public void surfaceChanged(android.view.SurfaceHolder h,int format,int w,int height) { requestSurfaceRate(h); }
                @Override public void surfaceDestroyed(android.view.SurfaceHolder h) { }
            });
        }
        // SDL may already have created its surface before this callback was added.
        requestSurfaceRate(mSurface.getHolder());
    }
    private void requestSurfaceRate(android.view.SurfaceHolder holder) {
        if(!holder.getSurface().isValid())return;
        // 60 fps content; once the panel refused 60 (checkRefresh), a vote for the 120 Hz
        // mode itself, so nothing here argues for the 90 the system fell back to.
        try {
            if(sixtyRefused) holder.getSurface().setFrameRate(120f,android.view.Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,android.view.Surface.CHANGE_FRAME_RATE_ALWAYS);
            else holder.getSurface().setFrameRate(60f,android.view.Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE,android.view.Surface.CHANGE_FRAME_RATE_ALWAYS);
        }
        catch(IllegalArgumentException|IllegalStateException e) {android.util.Log.w("SpeedBreakerOne","[display] setFrameRate: "+e);}
    }

    // The 60 Hz mode (preferSixtyHertz), asked again whenever the window comes
    // back: on the vivo X300 Pro the panel stayed at 90 Hz through a whole
    // session with the mode asked once. The mode id and the refresh rate both,
    // and the surface's own rate with CHANGE_FRAME_RATE_ALWAYS, so the system
    // switches even when the change isn't seamless.
    private android.view.Display.Mode sixtyHertz;

    // Did the panel take it? On the vivo X300 Pro it stayed at 90 Hz with 60 asked
    // every way Android offers. 2 s after asking, the rate is read back; still not a
    // multiple of 60, the 120 Hz mode is asked instead (each 60 fps frame then shows
    // for exactly two refreshes: smooth, unlike 90's 2-1-2-1). Every step is logged.
    private boolean sixtyRefused, displayWatched;
    private final Runnable refreshCheck = this::checkRefresh;

    private static boolean multipleOf60(float hz) {
        float times = hz / 60f;
        return Math.round(times) >= 1 && Math.abs(times - Math.round(times)) < 0.02f;
    }

    private void checkRefresh() {
        android.view.Display display = getDisplay();
        if (display == null || sixtyHertz == null) return;
        float hz = display.getRefreshRate();
        String line;
        if (multipleOf60(hz)) {
            getSharedPreferences("display_policy", MODE_PRIVATE).edit().remove("stuck_hz").apply();
            line = String.format(java.util.Locale.ROOT, "[display] panel at %.0f Hz (asked %.0f): even frame pacing", hz, sixtyHertz.getRefreshRate());
        } else if (!sixtyRefused) {
            sixtyRefused = true;
            line = String.format(java.util.Locale.ROOT, "[display] panel stayed at %.0f Hz (asked %.0f): asking for 120 Hz instead", hz, sixtyHertz.getRefreshRate());
            preferSixtyHertz();
        } else {
            getSharedPreferences("display_policy", MODE_PRIVATE).edit().putFloat("stuck_hz", hz).apply();  // for the launcher
            line = String.format(java.util.Locale.ROOT, "[display] panel still at %.0f Hz: the system keeps it there. Set the phone's refresh rate to 60 or 120 Hz in its display settings, or Frame Rate 30 (90 Hz paces 30 evenly)", hz);
        }
        android.util.Log.i("SpeedBreakerOne", line);
        DownloadsLog.note(line);
    }

    /** Every refresh-rate change of the panel while the game runs, in the log. */
    private void watchDisplay(android.view.Display display) {
        if (displayWatched) return;
        displayWatched = true;
        final int id = display.getDisplayId();
        getSystemService(android.hardware.display.DisplayManager.class).registerDisplayListener(new android.hardware.display.DisplayManager.DisplayListener() {
            private float last = display.getRefreshRate();
            @Override public void onDisplayChanged(int changed) {
                if (changed != id) return;
                android.view.Display d = getDisplay();
                float hz = d != null ? d.getRefreshRate() : last;
                if (Math.abs(hz - last) < 0.5f) return;
                DownloadsLog.note(String.format(java.util.Locale.ROOT, "[display] panel %.0f -> %.0f Hz", last, hz));
                last = hz;
            }
            @Override public void onDisplayAdded(int added) { }
            @Override public void onDisplayRemoved(int removed) { }
        }, new android.os.Handler(android.os.Looper.getMainLooper()));
    }

    private void applySixtyHertz() {
        if (sixtyHertz == null) return;
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.preferredDisplayModeId = sixtyHertz.getModeId();
        params.preferredRefreshRate = sixtyHertz.getRefreshRate();
        getWindow().setAttributes(params);
    }

    public String gpuDriverDirectory() { String s=getIntent().getStringExtra("driver_dir"); return s==null?"":s; }
    public String gpuDriverLibrary() { String s=getIntent().getStringExtra("driver_lib"); return s==null?"":s; }
    public String gpuDriverName() { String s=getIntent().getStringExtra("driver_name"); return s==null?"System":s; }
    public String gpuHookDirectory() { return getApplicationInfo().nativeLibraryDir; }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (request != DISC_REQUEST) { super.onActivityResult(request, result, data); return; }
        if (result != RESULT_OK || data == null || data.getData() == null) {
            nativeDiscResult(-1, "", "");
            return;
        }
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            int fd = openDisc(uri);
            nativeDiscResult(fd, pendingName, "");
        } catch (java.io.IOException | RuntimeException e) {
            pendingUri = uri.toString();
            abandonDisc();
            nativeDiscResult(-1, "", "Cannot read the selected file. Choose a local ISO on internal storage or the SD card.");
        }
    }
    @Override protected String[] getLibraries() { return new String[] {"SDL3", "main"}; }
    // SDL also requests orientation while creating/reconfiguring the surface.
    // Keep that request consistent with the manifest: either landscape side,
    // independent of the user's portrait rotation lock, never portrait.
    @Override public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
    private void hideSystemBars() {
        getWindow().setDecorFitsSystemWindows(false);
        WindowInsetsController controller = getWindow().getInsetsController();
        if (controller != null) {
            controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            controller.hide(WindowInsets.Type.systemBars());
        }
    }
    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {hideSystemBars();applySixtyHertz();}
        else if (cheatsMenu != null) cheatsMenu.focusLost();
        if (touchControls != null) {
            if (hasFocus && resumed) touchControls.resume();
            else touchControls.pause();
        }
    }
}
