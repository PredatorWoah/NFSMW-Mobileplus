// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.Intent;
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
    private SharedPreferences prefs;
    private TextView status;
    private Button play;
    private boolean importing;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("gpu_driver", MODE_PRIVATE);
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(24 * getResources().getDisplayMetrics().density);
        panel.setPadding(pad, pad, pad, pad);
        TextView title = new TextView(this);
        title.setText("SpeedBreaker One"); title.setTextSize(26); panel.addView(title);
        status = new TextView(this); status.setTextSize(17); panel.addView(status);
        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        panel.addView(actions);
        Button drivers = new Button(this); drivers.setText("Select GPU driver");
        drivers.setOnClickListener(v -> selectDriver());
        play = new Button(this); play.setOnClickListener(v -> play(false));
        Button restart = new Button(this); restart.setText("Restart game");
        restart.setOnClickListener(v -> {
            if (gamePid() == 0) { play(true); return; }
            new AlertDialog.Builder(this).setMessage("Restart the running game? Unsaved race progress will be lost.")
                .setNegativeButton("Cancel", null).setPositiveButton("Restart", (d,w) -> play(true)).show();
        });
        for (Button action : new Button[] {drivers, play, restart}) {
            LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1);
            int gap = (int)(4 * getResources().getDisplayMetrics().density);
            size.setMargins(gap, gap * 3, gap, gap * 3);
            action.setMinHeight((int)(72 * getResources().getDisplayMetrics().density));
            actions.addView(action, size);
        }
        Button logs = new Button(this);
        logs.setOnClickListener(v -> { DownloadsLog.setEnabled(this, !DownloadsLog.enabled(this)); logsLabel(logs); });
        logsLabel(logs);
        panel.addView(logs);
        TextView help = new TextView(this);
        help.setText("Import an ARM64 Turnip driver ZIP with meta.json. System driver is always available. Driver changes apply to a new game process; Resume keeps its current driver.");
        panel.addView(help);
        android.widget.ScrollView scroll = new android.widget.ScrollView(this);
        scroll.setFillViewport(true);
        scroll.addView(panel);
        setContentView(scroll); refresh();
        // Why the last game process ended, if it ended badly (DownloadsLog). Reads logcat: off the UI thread.
        android.content.Context app = getApplicationContext();
        new Thread(() -> DownloadsLog.reportExits(app), "exit-report").start();
    }
    private void logsLabel(Button b) {
        b.setText(DownloadsLog.enabled(this) ? "Logs to Downloads: ON (SpeedBreaker-logs)" : "Logs to Downloads: OFF");
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
        status.setText("Selected driver: " + prefs.getString("name", "System") + (importing ? "\nImporting…" : ""));
        play.setText(gamePid() == 0 ? "Play" : "Resume game"); play.setEnabled(!importing);
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
        if (request!=IMPORT || result!=RESULT_OK || data==null || data.getData()==null) return;
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
    private void error(String message) { new AlertDialog.Builder(this).setTitle("GPU driver").setMessage(message).setPositiveButton("OK",null).show(); }
}
