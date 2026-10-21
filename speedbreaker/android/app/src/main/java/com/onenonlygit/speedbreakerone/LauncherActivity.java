// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import org.json.JSONObject;
import java.io.*;
import java.util.*;
import java.util.zip.*;

/** The selector stays outside the Vulkan process, including after a driver crash. */
public final class LauncherActivity extends Activity {
    private static final int IMPORT = 7312;
    /** From the game's CHEATS menu: restart the game process with the saved choices. */
    static final String EXTRA_RESTART = "restart_game";
    private SharedPreferences prefs;
    private TextView status;
    private Button play;
    private boolean importing;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("gpu_driver", MODE_PRIVATE);
        root = new android.widget.FrameLayout(this);
        backdrop = new MwTheme.Backdrop(this);
        root.addView(backdrop, new android.widget.FrameLayout.LayoutParams(-1, -1));
        home = buildHome();
        garage = buildGarage();
        root.addView(home, new android.widget.FrameLayout.LayoutParams(-1, -1));
        root.addView(garage, new android.widget.FrameLayout.LayoutParams(-1, -1));
        garage.setVisibility(android.view.View.GONE);
        setContentView(root);
        refresh();
        updatePinnedShortcut();  // a shortcut pinned by an older build takes the current icon
        // Why the last game process ended, if it ended badly (DownloadsLog). Reads logcat: off the UI thread.
        android.content.Context app = getApplicationContext();
        new Thread(() -> DownloadsLog.reportExits(app), "exit-report").start();
        boolean restarting = getIntent() != null && getIntent().getBooleanExtra(EXTRA_RESTART, false);
        restartIfAsked(getIntent());
        raceIfAsked(getIntent());
        showBadge(UpdateCheck.cachedNewer(this));
        if (!restarting && !BuildConfig.PLATFORM_PROBE) UpdateCheck.check(this, false, this::showBadge);
        play.requestFocus();
    }

    // --- Home: the disc's art, RACE, and the way to everything else.

    private android.widget.FrameLayout root;
    private android.view.View home, garage;
    private android.widget.ImageView art;
    private TextView title, running, badge;
    private Button driverRow, logsRow, displayRow, updatesRow, shortcutRow, tapRow, rumbleRow, restoreRow;
    private MwTheme.Backdrop backdrop;
    private boolean backdropSet;
    private android.window.OnBackInvokedCallback garageBack;

    private android.view.View buildHome() {
        int pad = MwTheme.dp(this, 28);
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(android.view.Gravity.CENTER_VERTICAL);
        row.setPadding(pad, pad, pad, pad);

        // Left: the game's own dashboard icon, from the player's disc (title_art in the game process).
        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        left.setGravity(android.view.Gravity.CENTER);
        art = new android.widget.ImageView(this);
        art.setScaleType(android.widget.ImageView.ScaleType.FIT_CENTER);
        int frame = MwTheme.dp(this, 4);
        art.setPadding(frame, frame, frame, frame);
        android.graphics.drawable.GradientDrawable border = new android.graphics.drawable.GradientDrawable();
        border.setColor(MwTheme.BLACK);
        border.setStroke(MwTheme.dp(this, 3), MwTheme.GOLD);
        border.setCornerRadius(MwTheme.dp(this, 6));
        art.setBackground(border);
        art.setElevation(MwTheme.dp(this, 12));
        int size = MwTheme.dp(this, 176);
        left.addView(art, new LinearLayout.LayoutParams(size, size));
        row.addView(left, new LinearLayout.LayoutParams(0, -1, 0.42f));

        // Right: title, state, RACE, and the secondary buttons.
        LinearLayout right = new LinearLayout(this);
        right.setOrientation(LinearLayout.VERTICAL);
        right.setGravity(android.view.Gravity.CENTER_VERTICAL);
        badge = MwTheme.text(this, "", 14, MwTheme.BLACK, MwTheme.DISPLAY);
        badge.setAllCaps(true);
        badge.setBackground(new MwTheme.Slant(this, MwTheme.GOLD, 0, 0));
        int bp = MwTheme.dp(this, 10);
        badge.setPadding(bp * 2, bp / 2, bp * 2, bp / 2);
        badge.setVisibility(android.view.View.GONE);
        badge.setFocusable(true);
        LinearLayout.LayoutParams badgeSize = new LinearLayout.LayoutParams(-2, -2);
        badgeSize.gravity = android.view.Gravity.END;
        right.addView(badge, badgeSize);
        title = MwTheme.text(this, "", 40, MwTheme.TEXT, MwTheme.DISPLAY);
        title.setAllCaps(true);
        title.setShadowLayer(MwTheme.dp(this, 10), 0, 0, Color.argb(160, 242, 183, 5));
        right.addView(title);
        running = MwTheme.text(this, "", 15, MwTheme.MUTED, MwTheme.BODY);
        right.addView(running);
        status = MwTheme.text(this, "", 14, MwTheme.MUTED, MwTheme.BODY);
        right.addView(status);

        play = MwTheme.primary(this, "Race");
        play.setOnClickListener(v -> play(false));
        LinearLayout.LayoutParams big = new LinearLayout.LayoutParams(-1, -2);
        big.topMargin = MwTheme.dp(this, 18);
        right.addView(play, big);

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        Button restart = MwTheme.secondary(this, "Restart");
        restart.setOnClickListener(v -> {
            if (gamePid() == 0) { play(true); return; }
            new AlertDialog.Builder(this).setMessage("Restart the running game? Unsaved race progress will be lost.")
                .setNegativeButton("Cancel", null).setPositiveButton("Restart", (d,w) -> play(true)).show();
        });
        Button garageButton = MwTheme.secondary(this, "Garage");
        garageButton.setOnClickListener(v -> showGarage(true));
        Button controls = MwTheme.secondary(this, "Controls");
        controls.setOnClickListener(v -> startActivity(new Intent(this, ControlsEditorActivity.class)));
        for (Button b : new Button[] {restart, garageButton, controls}) {
            LinearLayout.LayoutParams each = new LinearLayout.LayoutParams(0, -2, 1);
            each.setMargins(0, MwTheme.dp(this, 12), MwTheme.dp(this, 10), 0);
            actions.addView(b, each);
        }
        right.addView(actions, new LinearLayout.LayoutParams(-1, -2));
        row.addView(right, new LinearLayout.LayoutParams(0, -1, 0.58f));
        return row;
    }

    // --- Garage: everything that isn't racing.

    private android.view.View buildGarage() {
        int pad = MwTheme.dp(this, 28);
        LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        list.setPadding(pad, pad, pad, pad);
        TextView heading = MwTheme.text(this, "GARAGE", 34, MwTheme.GOLD, MwTheme.DISPLAY);
        list.addView(heading);
        driverRow = MwTheme.row(this);
        driverRow.setOnClickListener(v -> selectDriver());
        logsRow = MwTheme.row(this);
        logsRow.setOnClickListener(v -> { DownloadsLog.setEnabled(this, !DownloadsLog.enabled(this)); refresh(); });
        displayRow = MwTheme.row(this);
        SharedPreferences displayPrefs = getSharedPreferences("display_policy", MODE_PRIVATE);
        displayRow.setOnClickListener(v -> { displayPrefs.edit().putBoolean("prefer60", !displayPrefs.getBoolean("prefer60", true)).commit(); refresh(); });
        updatesRow = MwTheme.row(this);
        updatesRow.setOnClickListener(v -> UpdateCheck.check(this, true));
        shortcutRow = MwTheme.row(this);
        shortcutRow.setOnClickListener(v -> pinShortcut());
        // Haptics (TouchGamepadView): a tick per button press, and the game's rumble on the phone.
        SharedPreferences haptics = getSharedPreferences(TouchGamepadView.HAPTICS, MODE_PRIVATE);
        tapRow = MwTheme.row(this);
        tapRow.setOnClickListener(v -> { haptics.edit().putBoolean("tap", !haptics.getBoolean("tap", true)).commit(); refresh(); });
        rumbleRow = MwTheme.row(this);
        rumbleRow.setOnClickListener(v -> { haptics.edit().putBoolean("rumble", !haptics.getBoolean("rumble", true)).commit(); refresh(); });
        restoreRow = MwTheme.row(this);
        restoreRow.setOnClickListener(v -> restoreSave());
        Button back = MwTheme.secondary(this, "Back");
        back.setOnClickListener(v -> showGarage(false));
        for (Button b : new Button[] {shortcutRow, tapRow, rumbleRow, driverRow, logsRow, displayRow, restoreRow, updatesRow}) {
            LinearLayout.LayoutParams each = new LinearLayout.LayoutParams(-1, -2);
            each.topMargin = MwTheme.dp(this, 10);
            list.addView(b, each);
        }
        TextView help = MwTheme.text(this, "GPU driver: System, or an ARM64 Turnip ZIP with meta.json. Driver, display and haptics changes apply to a new game process; Resume keeps the running one.",
                13, MwTheme.MUTED, MwTheme.BODY);
        help.setPadding(0, MwTheme.dp(this, 14), 0, MwTheme.dp(this, 6));
        list.addView(help);
        LinearLayout.LayoutParams backSize = new LinearLayout.LayoutParams(MwTheme.dp(this, 220), -2);
        list.addView(back, backSize);
        android.widget.ScrollView scroll = new android.widget.ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(list);
        return scroll;
    }

    private void showGarage(boolean show) {
        home.setVisibility(show ? android.view.View.GONE : android.view.View.VISIBLE);
        garage.setVisibility(show ? android.view.View.VISIBLE : android.view.View.GONE);
        if (show) {
            refresh();
            shortcutRow.requestFocus();
            if (garageBack == null) {
                garageBack = () -> showGarage(false);
                getOnBackInvokedDispatcher().registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, garageBack);
            }
        } else {
            play.requestFocus();
            if (garageBack != null) { getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(garageBack); garageBack = null; }
        }
    }

    private void showBadge(JSONObject latest) {
        if (badge == null) return;
        if (latest == null) { badge.setVisibility(android.view.View.GONE); return; }
        badge.setText("Update " + latest.optString("versionName") + " available ›");
        badge.setOnClickListener(v -> UpdateCheck.offer(this, latest));
        badge.setVisibility(android.view.View.VISIBLE);
    }

    // The disc's art and name, saved by the game process (runtime/user/title_art.cpp).
    private File userFile(String name) { return new File(new File(new File(getFilesDir(), "data"), "speedbreaker"), name); }

    // The cover art, in the game APK when it was built with private/cover.jpg
    // (scripts/android/prepare_disc_icon.sh): resources that exist only there,
    // so looked up by name. 0 when absent.
    private int coverResource(String name) { return getResources().getIdentifier(name, "drawable", getPackageName()); }

    private android.graphics.Bitmap titleIcon(int size) {
        int cover = coverResource("launcher_art");
        android.graphics.Bitmap icon = cover != 0 ? android.graphics.BitmapFactory.decodeResource(getResources(), cover) : null;
        if (icon == null) icon = android.graphics.BitmapFactory.decodeFile(userFile("title-icon.png").getPath());
        return icon == null ? null : android.graphics.Bitmap.createScaledBitmap(icon, size, size, true);
    }

    private String titleName() {
        try { return new String(java.nio.file.Files.readAllBytes(userFile("title.txt").toPath()), java.nio.charset.StandardCharsets.UTF_8).trim(); }
        catch (IOException e) { return ""; }
    }

    // --- Save backups (runtime/user/save_backup.cpp makes one at each game start).

    private File[] saveBackups() {
        File[] dirs = userFile("save-backups").listFiles(f -> f.isDirectory() && !f.getName().endsWith(".partial"));
        if (dirs == null) return new File[0];
        java.util.Arrays.sort(dirs, (a, b) -> b.getName().compareTo(a.getName()));  // newest first
        return dirs;
    }

    private void restoreSave() {
        File[] backups = saveBackups();
        if (backups.length == 0) { error("No save backups yet. One is made each time the game starts."); return; }
        String[] names = new String[backups.length];
        for (int i = 0; i < backups.length; i++) names[i] = backups[i].getName().replace('_', ' ');
        new AlertDialog.Builder(this).setTitle("Restore which save?").setItems(names, (d, which) ->
            new AlertDialog.Builder(this).setMessage("Replace your current save with the one from " + names[which] + "?"
                    + (gamePid() != 0 ? " The running game will be closed first (unsaved race progress is lost)." : "")
                    + " Your current save is backed up again the next time the game starts.")
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Restore", (d2, w) -> restoreFrom(backups[which])).show()
        ).setNegativeButton("Cancel", null).show();
    }

    private void restoreFrom(File backup) {
        int pid = gamePid();
        if (pid != 0) {
            ActivityManager manager = (ActivityManager)getSystemService(ACTIVITY_SERVICE);
            for (ActivityManager.AppTask task : manager.getAppTasks()) {
                android.content.ComponentName base = task.getTaskInfo().baseActivity;
                if (base != null && base.getClassName().equals(SpeedBreakerActivity.class.getName())) task.finishAndRemoveTask();
            }
            Process.killProcess(pid);
        }
        new Thread(() -> {
            for (int i = 0; i < 100 && gamePid() != 0; ++i) try { Thread.sleep(20); } catch (InterruptedException e) { return; }
            File save = userFile("save"), old = userFile("save.replaced");
            String failure = null;
            try {
                deleteTree(old);
                if (save.exists() && !save.renameTo(old)) throw new IOException("Cannot move the current save aside.");
                copyTree(backup, save);
                deleteTree(old);
            } catch (IOException e) {
                failure = e.getMessage();
                deleteTree(save);
                old.renameTo(save);  // put the current one back
            }
            final String message = failure == null ? "Save restored from " + backup.getName().replace('_', ' ') + "." : "Restore failed: " + failure;
            runOnUiThread(() -> { refresh(); error(message); });
        }, "save-restore").start();
    }

    private static void copyTree(File from, File to) throws IOException {
        if (from.isDirectory()) {
            if (!to.isDirectory() && !to.mkdirs()) throw new IOException("Cannot create " + to.getName());
            File[] children = from.listFiles();
            if (children != null) for (File c : children) copyTree(c, new File(to, c.getName()));
        } else {
            java.nio.file.Files.copy(from.toPath(), to.toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private static void deleteTree(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File c : children) deleteTree(c);
        f.delete();
    }

    /** A home-screen shortcut with the disc's own icon that goes straight to the race. */
    private android.content.pm.ShortcutInfo raceShortcut(android.graphics.Bitmap icon) {
        String name = titleName().isEmpty() ? "SpeedBreaker" : titleName().replace("NFS ", "");
        Intent race = new Intent(this, LauncherActivity.class).setAction(Intent.ACTION_MAIN).putExtra(EXTRA_RACE, true)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return new android.content.pm.ShortcutInfo.Builder(this, "race")
                .setShortLabel(name).setLongLabel(name + ": race")
                .setIcon(android.graphics.drawable.Icon.createWithAdaptiveBitmap(adaptive(icon))).setIntent(race).build();
    }

    /** The icon in an adaptive square: the picture fills the visible centre (72 of 108), black around it. */
    private static android.graphics.Bitmap adaptive(android.graphics.Bitmap icon) {
        int full = 432, inner = 288;
        android.graphics.Bitmap out = android.graphics.Bitmap.createBitmap(full, full, android.graphics.Bitmap.Config.ARGB_8888);
        android.graphics.Canvas canvas = new android.graphics.Canvas(out);
        canvas.drawColor(MwTheme.BLACK);
        int o = (full - inner) / 2;
        canvas.drawBitmap(icon, null, new android.graphics.Rect(o, o, o + inner, o + inner), new android.graphics.Paint(android.graphics.Paint.FILTER_BITMAP_FLAG));
        return out;
    }

    /** The shortcut already on the home screen, if any, with the current icon. */
    private void updatePinnedShortcut() {
        android.content.pm.ShortcutManager shortcuts = getSystemService(android.content.pm.ShortcutManager.class);
        android.graphics.Bitmap icon = titleIcon(512);
        if (shortcuts == null || icon == null) return;
        for (android.content.pm.ShortcutInfo pinned : shortcuts.getPinnedShortcuts())
            if (pinned.getId().equals("race")) shortcuts.updateShortcuts(java.util.Collections.singletonList(raceShortcut(icon)));
    }

    private void pinShortcut() {
        android.graphics.Bitmap icon = titleIcon(512);
        android.content.pm.ShortcutManager shortcuts = getSystemService(android.content.pm.ShortcutManager.class);
        if (icon == null) { error("Start the game once first: its icon comes from your disc."); return; }
        if (shortcuts == null || !shortcuts.isRequestPinShortcutSupported()) { error("This home screen doesn't take shortcuts."); return; }
        shortcuts.requestPinShortcut(raceShortcut(icon), null);
    }

    /** From the home-screen shortcut: straight into the game (or back to it). */
    static final String EXTRA_RACE = "race";
    private void raceIfAsked(Intent intent) {
        if (intent == null || !intent.getBooleanExtra(EXTRA_RACE, false)) return;
        intent.removeExtra(EXTRA_RACE);
        play(false);
    }

    // A gamepad drives the launcher too: A presses, B goes back.
    @Override public boolean dispatchKeyEvent(android.view.KeyEvent event) {
        int key = event.getKeyCode();
        if (key == android.view.KeyEvent.KEYCODE_BUTTON_A || key == android.view.KeyEvent.KEYCODE_BUTTON_B) {
            if (event.getAction() != android.view.KeyEvent.ACTION_UP) return true;
            if (key == android.view.KeyEvent.KEYCODE_BUTTON_B) {
                if (garage.getVisibility() == android.view.View.VISIBLE) showGarage(false);
                return true;
            }
            android.view.View focused = getCurrentFocus();
            if (focused == null) { (garage.getVisibility() == android.view.View.VISIBLE ? shortcutRow : play).requestFocus(); return true; }
            focused.performClick();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) return;
        getWindow().setDecorFitsSystemWindows(false);
        android.view.WindowInsetsController c = getWindow().getInsetsController();
        if (c != null) {
            c.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            c.hide(android.view.WindowInsets.Type.systemBars());
        }
    }
    @Override protected void onNewIntent(Intent intent) { super.onNewIntent(intent); restartIfAsked(intent); raceIfAsked(intent); }
    private void restartIfAsked(Intent intent) {
        if (intent == null || !intent.getBooleanExtra(EXTRA_RESTART, false)) return;
        intent.removeExtra(EXTRA_RESTART);  // not again on rotation or recreation
        play(true);
    }
    @Override protected void onResume() { super.onResume(); if (status != null) refresh(); }
    private int gamePid() {
        ActivityManager manager = (ActivityManager)getSystemService(ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> list = manager.getRunningAppProcesses();
        if (list != null) for (ActivityManager.RunningAppProcessInfo p : list)
            if (p.uid == Process.myUid() && p.processName.equals(getPackageName() + ":game")) return p.pid;
        return 0;
    }
    private void refresh() {
        boolean live = gamePid() != 0;
        android.graphics.Bitmap icon = titleIcon(512);
        if (icon != null) art.setImageBitmap(icon); else art.setImageResource(R.mipmap.ic_launcher);
        if (!backdropSet) {
            backdropSet = true;
            int background = coverResource("launcher_background");
            if (background != 0) backdrop.setPicture(android.graphics.BitmapFactory.decodeResource(getResources(), background));
        }
        String name = titleName();
        title.setText(name.isEmpty() ? "SpeedBreaker One" : name);
        // A test APK (-PsbDiagnostics=ON) has no game in it: say so, and what its button really does.
        running.setText(BuildConfig.PLATFORM_PROBE ? "Test build: no game inside. It checks this phone and lets you edit the touch controls."
                : live ? "● Game running" : icon == null ? "Pick your Most Wanted (USA) disc image on the first race" : "Ready");
        running.setTextColor(live ? MwTheme.GOLD : MwTheme.MUTED);
        float stuck = getSharedPreferences("display_policy", MODE_PRIVATE).getFloat("stuck_hz", 0);
        status.setText((stuck > 0 ? String.format(java.util.Locale.ROOT, "The phone kept the screen at %.0f Hz last time: set 60 or 120 Hz in its display settings for smooth frames\n", stuck) : "")
                + "SpeedBreaker One " + BuildConfig.VERSION_NAME + "  ·  GPU: " + prefs.getString("name", "System") + (importing ? "  ·  importing…" : ""));
        play.setText(BuildConfig.PLATFORM_PROBE ? "Run phone test" : live ? "Resume" : "Race");
        play.setEnabled(!importing);
        if (driverRow == null) return;
        SharedPreferences displayPrefs = getSharedPreferences("display_policy", MODE_PRIVATE);
        driverRow.setText("GPU driver:  " + prefs.getString("name", "System"));
        logsRow.setText("Logs to Downloads:  " + (DownloadsLog.enabled(this) ? "on (Download/SpeedBreaker-logs)" : "off"));
        displayRow.setText("60 Hz display request:  " + (displayPrefs.getBoolean("prefer60", true) ? "on" : "off"));
        updatesRow.setText("Check for updates  (" + BuildConfig.VERSION_NAME + ")");
        File[] backups = saveBackups();
        restoreRow.setText("Restore save:  " + (backups.length == 0 ? "no backups yet (made at each game start)" : backups.length + " backup" + (backups.length > 1 ? "s" : "") + ", newest " + backups[0].getName().replace('_', ' ')));
        SharedPreferences haptics = getSharedPreferences(TouchGamepadView.HAPTICS, MODE_PRIVATE);
        tapRow.setText("Touch button taps (haptics):  " + (haptics.getBoolean("tap", true) ? "on" : "off"));
        rumbleRow.setText("Game rumble on the phone:  " + (haptics.getBoolean("rumble", true) ? "on" : "off") + "  (crashes, nitro, rough roads)");
        shortcutRow.setText("Add to home screen" + (icon == null ? "  (start the game once first)" : "  (a shortcut straight to the race)"));
    }
    private void play(boolean restart) {
        if (importing) return;
        int pid = gamePid();
        if (restart && pid != 0) {
            ActivityManager manager = (ActivityManager)getSystemService(ACTIVITY_SERVICE);
            for (ActivityManager.AppTask task : manager.getAppTasks()) {
                android.content.ComponentName base = task.getTaskInfo().baseActivity;
                if (base != null && base.getClassName().equals(SpeedBreakerActivity.class.getName())) task.finishAndRemoveTask();
            }
            Process.killProcess(pid);
            // Wait off the UI thread for process death before reusing the Activity.
            play.setEnabled(false);
            new Thread(() -> {
                for (int i=0; i<100 && gamePid()!=0; ++i) try { Thread.sleep(20); } catch (InterruptedException e) { return; }
                runOnUiThread(() -> { if (gamePid()==0) launch(); else { refresh(); error("Game is still closing. Try again."); } });
            }, "driver-restart").start();
        } else launch();
    }
    private void launch() {
        Intent intent = new Intent(this, SpeedBreakerActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra("driver_dir", prefs.getString("dir", ""));
        intent.putExtra("driver_lib", prefs.getString("lib", ""));
        intent.putExtra("driver_name", prefs.getString("name", "System"));
        startActivity(intent); refresh();
    }
    private void selectDriver() {
        if (importing) return;
        ArrayList<String> labels = new ArrayList<>(); ArrayList<File> files = new ArrayList<>();
        labels.add("System driver"); files.add(null);
        File root = new File(getFilesDir(), "gpu-drivers");
        File[] dirs = root.listFiles();
        if (dirs != null) { Arrays.sort(dirs); for (File dir : dirs) {
            try { if (!new File(dir,"complete").isFile()) continue; JSONObject m = readMeta(new File(dir,"meta.json"));
                String lib = libraryName(m);
                if (new File(dir, lib).isFile()) { labels.add(m.optString("name", lib)); files.add(dir); }
            } catch (Exception ignored) { }
        } }
        labels.add("Import driver ZIP…");
        new AlertDialog.Builder(this).setTitle("GPU driver (requires restart)")
            .setItems(labels.toArray(new String[0]), (dialog, which) -> {
                if (which == labels.size()-1) {
                    Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
                    startActivityForResult(pick, IMPORT);
                } else if (which == 0) {
                    prefs.edit().clear().commit(); refresh();
                } else try { choose(files.get(which), readMeta(new File(files.get(which), "meta.json"))); }
                catch (Exception e) { error(e.getMessage()); }
            }).setNegativeButton("Cancel", null).show();
    }
    private void choose(File dir, JSONObject meta) throws Exception {
        String lib = libraryName(meta);
        DriverArchive.validateElf(new File(dir,lib));
        if (meta.optInt("minApi", 0) > android.os.Build.VERSION.SDK_INT) throw new IOException("This driver needs a newer Android version.");
        prefs.edit().putString("dir", dir.getCanonicalPath()+"/").putString("lib", lib)
            .putString("name", meta.optString("name", lib)).commit(); refresh();
    }
    private static String libraryName(JSONObject meta) throws Exception {
        String lib = meta.getString("libraryName");
        if (!lib.matches("[A-Za-z0-9_+.-]+\\.so") || lib.contains("..")) throw new IOException("Invalid driver library name.");
        return lib;
    }
    private static JSONObject readMeta(File file) throws Exception {
        if (file.length() > 65536) throw new IOException("Driver metadata too large.");
        try (InputStream in = new FileInputStream(file); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] b = new byte[4096]; int n; while ((n=in.read(b))!=-1) { out.write(b,0,n); if (out.size()>65536) throw new IOException("Driver metadata too large."); }
            return new JSONObject(out.toString("UTF-8"));
        }
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request,result,data);
        if (result!=RESULT_OK || data==null || data.getData()==null) return;
        if (request!=IMPORT) return;
        Uri uri = data.getData(); importing=true; refresh();
        new Thread(() -> {
            File folder = new File(new File(getFilesDir(), "gpu-drivers"), UUID.randomUUID().toString());
            try {
                if (!folder.mkdirs()) throw new IOException("Cannot create driver folder.");
                try (InputStream source=getContentResolver().openInputStream(uri)) {
                    DriverArchive.extract(source, folder);
                }
                JSONObject meta=readMeta(new File(folder,"meta.json"));
                if (!new File(folder,libraryName(meta)).isFile()) throw new IOException("Driver library from meta.json is missing.");
                if (meta.optInt("minApi", 0) > android.os.Build.VERSION.SDK_INT) throw new IOException("This driver needs a newer Android version.");
                if (!new File(folder,"complete").createNewFile()) throw new IOException("Cannot finish driver import.");
                runOnUiThread(() -> { importing=false; try { choose(folder,meta); } catch (Exception e) { error(e.getMessage()); } refresh(); });
            } catch (Exception e) {
                File[] files=folder.listFiles(); if (files!=null) for (File f:files) f.delete(); folder.delete();
                runOnUiThread(() -> { importing=false; refresh(); error("Driver import failed: "+e.getMessage()); });
            }
        }, "driver-import").start();
    }
    private void error(String message) { new AlertDialog.Builder(this).setTitle("SpeedBreaker").setMessage(message).setPositiveButton("OK",null).show(); }
}
