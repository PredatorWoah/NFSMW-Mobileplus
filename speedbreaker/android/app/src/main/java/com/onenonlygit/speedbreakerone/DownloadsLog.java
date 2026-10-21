// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Environment;
import android.os.PowerManager;
import android.provider.MediaStore;
import android.util.Log;
import android.view.Display;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.*;

/**
 * Everything from a game or probe session in Download/SpeedBreaker-logs, to
 * send to whoever is diagnosing a phone they don't have (ported from
 * NFSMW-Mobileplus's RegistroDescargas). Per session:
 *
 *   sb_<time>_<kind>.txt         device/SoC/cores/display header, this app's
 *                                logcat while it runs (SDL, Java, the Vulkan
 *                                driver) and a "diag" line every 10 s: thermal
 *                                status and headroom, battery temperature, free
 *                                RAM, RSS, display Hz and each core's MHz
 *   sb_<time>_<kind>_<log>       the native session log (files/data/speedbreaker/logs, where
 *                                stderr goes) and android-diagnostics.log,
 *                                copied every second as they grow
 *   hang-*.txt, crash-*.txt,     the runtime's hang and crash reports and the
 *   speedbreaker-bug-report-*.zip  Save Bug Report zips, whole, once each (also
 *                                one a crashed session left, at the next start)
 *
 * and, when the launcher opens after a game process ended badly (crash, native
 * crash, ANR, low memory), sb_<time>_exit.txt with Android's reason, the logcat
 * that was left and, for a native crash, the tombstone (.pb).
 *
 * Everything is flushed as it arrives, so it survives a crash. Written through
 * MediaStore (no permission on API 29+); if that is refused, to the app's
 * external files dir. On by default; the launcher's "Logs to Downloads" turns it off.
 */
final class DownloadsLog {
    private static final String TAG = "SpeedBreakerOne-diag";
    private static final String FOLDER = Environment.DIRECTORY_DOWNLOADS + "/SpeedBreaker-logs";
    private static final String PREFS = "downloads_log";
    private static DownloadsLog current;

    private final Context ctx;
    private final OutputStream out;
    private final String base;
    private final long started = System.currentTimeMillis();
    private final Map<File, OutputStream> copies = new HashMap<>();
    private final Map<File, Long> copied = new HashMap<>();
    private String since;
    private long logcatLines;
    private Process logcat;
    private Thread logcatThread, copyThread;
    private volatile boolean stopping;
    private PowerManager.OnThermalStatusChangedListener thermalListener;
    private long bytesWritten, logTextBytes;
    private static final long SESSION_LIMIT=64L<<20, FILE_LIMIT=16L<<20;

    private DownloadsLog(Context ctx, OutputStream out, String base) { this.ctx = ctx; this.out = out; this.base = base; }

    static boolean enabled(Context ctx) { return ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean("enabled", true); }
    static void setEnabled(Context ctx, boolean on) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean("enabled", on).apply(); }

    /** From the game Activity's onCreate, before SDL loads the native libraries. kind: "game" or "probe". */
    static synchronized void start(Context ctx, String kind) {
        if (current != null || !enabled(ctx)) return;
        String base = "sb_" + stamp(new Date()) + "_" + kind;
        OutputStream out = create(ctx, base + ".txt");
        if (out == null) return;
        DownloadsLog d = new DownloadsLog(ctx.getApplicationContext(), out, base);
        d.write(header(ctx, kind));
        d.run();
        current = d;
    }

    /** A line of the app's own into the session file (not only logcat). */
    static synchronized void note(String line) {
        if (current != null) current.diag(line);
    }

    /** From onDestroy, before the process is killed. */
    static synchronized void stop(Runnable complete) {
        DownloadsLog d = current;
        current = null;
        java.util.concurrent.atomic.AtomicBoolean done=new java.util.concurrent.atomic.AtomicBoolean();
        Runnable once=() -> {if(done.compareAndSet(false,true))complete.run();};
        Thread cleanup=new Thread(() -> {try {if(d!=null)d.finish();}finally {once.run();}},"SB log shutdown");
        cleanup.start();
        // Process teardown must not depend on a logcat pipe or slow storage.
        Thread deadline=new Thread(() -> {try {Thread.sleep(1500);}catch(InterruptedException ignored){}once.run();},"SB log shutdown deadline");
        deadline.setDaemon(true);deadline.start();
    }

    private void run() {
        Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            StringWriter sw = new StringWriter();
            e.printStackTrace(new PrintWriter(sw));
            write("\n=== JAVA CRASH in thread '" + t.getName() + "' ===\n" + sw + "\n");
            if (previous != null) previous.uncaughtException(t, e);
        });
        PowerManager pm = ctx.getSystemService(PowerManager.class);
        if (pm != null) {
            thermalListener=s -> write("[device] thermal status -> "+thermal(s)+"\n");
            pm.addThermalStatusListener(ctx.getMainExecutor(),thermalListener);
        }

        // Only this app's lines (Android allows no others), from now on.
        since = String.format(Locale.ROOT, "%d.%03d", started / 1000, started % 1000);
        logcatThread = new Thread(() -> {
            try {
                logcat = new ProcessBuilder("logcat", "-v", "threadtime", "-T", since).redirectErrorStream(true).start();
                try (BufferedReader in = new BufferedReader(new InputStreamReader(logcat.getInputStream(), StandardCharsets.UTF_8), 1 << 16)) {
                    StringBuilder block = new StringBuilder(1 << 14);
                    String line;
                    while (!stopping && (line = in.readLine()) != null) {
                        block.append(line).append('\n');
                        logcatLines++;
                        if (!in.ready() || block.length() > (1 << 14)) { write(block.toString()); block.setLength(0); }
                    }
                    if (block.length() > 0) write(block.toString());
                }
            } catch (IOException e) {
                // destroy() at the end cuts the read short: not a failure.
                if (!stopping) write("\n[log] logcat unavailable: " + e + "\n");
            }
        }, "SB downloads logcat");
        logcatThread.setDaemon(true);
        logcatThread.start();

        copyThread = new Thread(() -> {
            int tick = 0;
            while (!stopping) {
                copyNativeLogs();
                if (++tick % 10 == 0) write("[device] "+stamp(new Date())+" "+sample()+"\n");
                try { Thread.sleep(1000); } catch (InterruptedException e) { break; }
            }
            copyNativeLogs();
            for(OutputStream o:copies.values())close(o);
        }, "SB downloads copy");
        copyThread.setDaemon(true);
        copyThread.start();
    }

    private void finish() {
        write("[device] session end: "+sample()+"\n");
        stopping = true;
        if (copyThread != null) { copyThread.interrupt(); join(copyThread, 400); }
        if (logcat != null) logcat.destroy();
        if (logcatThread != null) join(logcatThread, 200);
        write("\n=== end " + stamp(new Date()) + " ===\n");
        PowerManager pm=ctx.getSystemService(PowerManager.class);
        if(pm!=null && thermalListener!=null)pm.removeThermalStatusListener(thermalListener);
        close(out);
        // The copier owns its streams. Do not race a still-running final copy.
        if(copyThread==null || !copyThread.isAlive())for(OutputStream o:copies.values())close(o);
    }

    /**
     * A diag line straight into the file, and to logcat. Not only to logcat:
     * in the game process the logcat stream delivered nothing on a Vivo X300
     * Pro (Android 17) while the probe's did, so these lines were lost.
     */
    private void diag(String line) {
        Log.i(TAG, line);
        write(new java.text.SimpleDateFormat("MM-dd HH:mm:ss.SSS", Locale.ROOT).format(new Date()) + " [diag] " + line + "\n");
    }

    /**
     * The native session log (<files>/data/speedbreaker/logs/<stamp>.log, stderr goes there:
     * runtime/report/log.cpp) and the probe's android-diagnostics.log, from the
     * ones this session wrote: modified since it started.
     */
    private void copyNativeLogs() {
        // <files>/data/speedbreaker/logs (user/paths.h: UserDataBase()/kUserFolderName),
        // or data/nfsmw-recomp/logs where an old folder could not be renamed.
        List<File> files = new ArrayList<>();
        // Also the hang and crash reports beside them (runtime/report/watchdog.cpp,
        // crash.cpp) and the Save Bug Report zips in the user folder
        // (bug_report.cpp; written to a .partial and renamed, so whole).
        for (String user : new String[] {"speedbreaker", "nfsmw-recomp"}) {
            File folder = new File(new File(ctx.getFilesDir(), "data"), user);
            File[] logs = new File(folder, "logs").listFiles((dir, name) -> name.endsWith(".log")
                    || ((name.startsWith("hang-") || name.startsWith("crash-")) && name.endsWith(".txt")));
            if (logs != null) files.addAll(Arrays.asList(logs));
            File[] reports = folder.listFiles((dir, name) -> name.startsWith("speedbreaker-bug-report-") && name.endsWith(".zip"));
            if (reports != null) files.addAll(Arrays.asList(reports));
        }
        files.add(new File(ctx.getFilesDir(), "android-diagnostics.log"));
        for (File f : files) {
            if (!f.isFile()) continue;
            if (!f.getName().endsWith(".log")) {
                copyReport(f);
                continue;
            }
            if (!copies.containsKey(f) && f.lastModified() < started - 2000) continue;
            long from = copied.getOrDefault(f, 0L), length = f.length();
            if (length < from) from = 0;
            if (length == from) continue;
            OutputStream o = copies.get(f);
            if (o == null) {
                o = bounded(create(ctx, base + "_" + f.getName()));
                if (o == null) continue;
                copies.put(f, o);
            }
            try (RandomAccessFile in = new RandomAccessFile(f, "r")) {
                in.seek(from);
                byte[] buf = new byte[1 << 16];
                for (int n; (n = in.read(buf)) > 0; from += n) o.write(buf, 0, n);
                o.flush();
            } catch (IOException e) {
                Log.w(TAG, "could not copy " + f + ": " + e);
            }
            copied.put(f, from);
        }
    }

    /**
     * A hang or crash report or a bug report zip, whole, once: also one an
     * earlier session left (a crash report is written as the process dies, so
     * only the next session can copy it). The names already copied are kept in
     * the prefs; reports older than 3 days are left alone.
     */
    private void copyReport(File f) {
        long age = System.currentTimeMillis() - f.lastModified();
        if (age < 2000 || age > 3L * 24 * 3600 * 1000) return;  // still being written, or old
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        Set<String> done = new HashSet<>(p.getStringSet("reports", Collections.emptySet()));
        if (done.contains(f.getName())) return;
        // Preserve complete report files. Do not truncate a ZIP at the stream
        // quota, and do not mark a failed copy as delivered.
        long size=f.length();
        if(size>FILE_LIMIT)return;
        synchronized(this) {
            if(size>SESSION_LIMIT-bytesWritten)return;
            OutputStream o=create(ctx,f.getName());
            if(o==null)return;
            try {
                long written=Files.copy(f.toPath(),o);
                bytesWritten+=written;
                o.flush();
                if(written!=size || f.length()!=size)return;
            }catch(IOException e) {
                Log.w(TAG,"could not copy "+f+": "+e);return;
            }finally {close(o);}
        }
        done.add(f.getName());
        p.edit().putStringSet("reports", done).apply();
        diag("copied " + f.getName() + " to Download/SpeedBreaker-logs");
    }

    private String sample() {
        StringBuilder s = new StringBuilder("diag");
        PowerManager pm = ctx.getSystemService(PowerManager.class);
        if (pm != null) {
            s.append(" thermal=").append(thermal(pm.getCurrentThermalStatus()));
            float headroom = pm.getThermalHeadroom(0);
            if (!Float.isNaN(headroom)) s.append(String.format(Locale.ROOT, " headroom=%.2f", headroom));
        }
        Intent battery = ctx.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (battery != null)
            s.append(String.format(Locale.ROOT, " battery=%d%% %.1fC", battery.getIntExtra(BatteryManager.EXTRA_LEVEL, -1),
                    battery.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) / 10f));
        ActivityManager am = ctx.getSystemService(ActivityManager.class);
        if (am != null) {
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            s.append(" ram_free=").append(mi.availMem >> 20).append("MB").append(mi.lowMemory ? " (LOW)" : "");
        }
        Display display = ctx.getSystemService(DisplayManager.class).getDisplay(Display.DEFAULT_DISPLAY);
        if (display != null) s.append(String.format(Locale.ROOT, " hz=%.0f", display.getRefreshRate()));
        for (String l : read("/proc/self/status").split("\n"))
            if (l.startsWith("VmRSS:")) s.append(" rss=").append(number(l.replaceAll("[^0-9]", "")) >> 10).append("MB");
        String gpu = gpuClock();
        if (!gpu.isEmpty()) s.append(" gpu=").append(gpu);
        s.append(" mhz=");
        for (int c = 0; c < Runtime.getRuntime().availableProcessors(); c++) {
            long khz = number(read("/sys/devices/system/cpu/cpu" + c + "/cpufreq/scaling_cur_freq"));
            s.append(c == 0 ? "" : ",").append(khz < 0 ? "?" : String.valueOf(khz / 1000));
        }
        return s.toString();
    }

    // The GPU's clock where the kernel lets an app read it: MediaTek's GED
    // (kHz or MHz, as the driver writes it), or a Mali devfreq node (Hz).
    // Probed once; most phones refuse them all, and then nothing is added.
    private static final String[] GPU_CLOCKS = {
        "/sys/kernel/ged/hal/current_freqency", "/sys/kernel/ged/hal/current_frequency",
        "/sys/class/misc/mali0/device/devfreq/mali0/cur_freq", "/sys/class/devfreq/mali0/cur_freq",
        "/sys/class/devfreq/13000000.mali/cur_freq",
    };
    private String gpuClockPath;

    private String gpuClock() {
        if (gpuClockPath == null) {
            gpuClockPath = "";
            for (String p : GPU_CLOCKS) if (!read(p).isEmpty()) { gpuClockPath = p; break; }
            diag("gpu clock: " + (gpuClockPath.isEmpty() ? "not readable" : gpuClockPath));
        }
        String v = gpuClockPath.isEmpty() ? "" : read(gpuClockPath).replaceAll("\\s+", " ");
        return v.length() > 40 ? v.substring(0, 40) : v;
    }

    private static String header(Context ctx, String kind) {
        StringBuilder s = new StringBuilder();
        s.append("=== SpeedBreaker One ").append(kind).append(' ').append(stamp(new Date())).append(" ===\n");
        s.append("app: ").append(BuildConfig.VERSION_NAME).append(" (").append(BuildConfig.VERSION_CODE)
                .append(") probe=").append(BuildConfig.PLATFORM_PROBE).append('\n');
        s.append("device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(" (").append(Build.DEVICE)
                .append(", ").append(Build.HARDWARE).append(")\n");
        s.append("soc: ").append(Build.SOC_MANUFACTURER).append(' ').append(Build.SOC_MODEL).append('\n');
        s.append("android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT)
                .append(") patch ").append(Build.VERSION.SECURITY_PATCH).append('\n');
        s.append("build: ").append(Build.FINGERPRINT).append('\n');
        ActivityManager am = ctx.getSystemService(ActivityManager.class);
        if (am != null) {
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            s.append("ram: ").append(mi.totalMem >> 20).append(" MB, free ").append(mi.availMem >> 20).append(" MB\n");
        }
        int cores = Runtime.getRuntime().availableProcessors();
        s.append("cores: ").append(cores).append('\n');
        for (int c = 0; c < cores; c++) {
            String d = "/sys/devices/system/cpu/cpu" + c + "/";
            s.append("  cpu").append(c).append(" capacity=").append(read(d + "cpu_capacity"))
                    .append(" max_khz=").append(read(d + "cpufreq/cpuinfo_max_freq")).append('\n');
        }
        Display display = ctx.getSystemService(DisplayManager.class).getDisplay(Display.DEFAULT_DISPLAY);
        if (display != null) {
            s.append("display:");
            for (Display.Mode m : display.getSupportedModes())
                s.append(String.format(Locale.ROOT, " %dx%d@%.0f", m.getPhysicalWidth(), m.getPhysicalHeight(), m.getRefreshRate()));
            s.append(String.format(Locale.ROOT, " (now %.0f Hz)\n", display.getRefreshRate()));
        }
        s.append("GPU, driver and renderer choices: see the _<stamp>.log native session log.\n=== logcat ===\n");
        return s.toString();
    }

    /** From the launcher: why recent game processes ended, if not normally. */
    static void reportExits(Context ctx) {
        if (!enabled(ctx)) return;
        ActivityManager am = ctx.getSystemService(ActivityManager.class);
        if (am == null) return;
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        long seen = p.getLong("exit_seen", 0), newest = seen;
        for (ApplicationExitInfo e : am.getHistoricalProcessExitReasons(null, 0, 16)) {
            if (e.getTimestamp() <= seen) continue;
            newest = Math.max(newest, e.getTimestamp());
            int r = e.getReason();
            // The game Activity kills its own process on exit (SIGKILL, 9).
            boolean normal = r == ApplicationExitInfo.REASON_EXIT_SELF || r == ApplicationExitInfo.REASON_USER_REQUESTED
                    || (r == ApplicationExitInfo.REASON_SIGNALED && e.getStatus() == 9);
            if (!normal) writeExit(ctx, e);
        }
        p.edit().putLong("exit_seen", newest).apply();
    }

    private static void writeExit(Context ctx, ApplicationExitInfo e) {
        String base = "sb_" + stamp(new Date(e.getTimestamp())) + "_exit";
        OutputStream o = create(ctx, base + ".txt");
        if (o == null) return;
        StringBuilder s = new StringBuilder();
        s.append("=== process ").append(e.getProcessName()).append(" ended ===\n")
                .append("when: ").append(new Date(e.getTimestamp())).append('\n')
                .append("reason: ").append(reason(e.getReason())).append(" (").append(e.getReason()).append(") status=")
                .append(e.getStatus()).append(" importance=").append(e.getImportance()).append('\n')
                .append("description: ").append(e.getDescription()).append('\n')
                .append("pss=").append(e.getPss() >> 10).append("MB rss=").append(e.getRss() >> 10).append("MB\n");
        try (InputStream trace = e.getTraceInputStream()) {
            if (trace != null) {
                byte[] data = trace.readNBytes(4<<20);
                if (e.getReason() == ApplicationExitInfo.REASON_CRASH_NATIVE) {
                    OutputStream t = create(ctx, base + "_tombstone.pb");
                    if (t != null) { t.write(data); close(t); s.append("tombstone: ").append(base).append("_tombstone.pb\n"); }
                } else {
                    s.append("=== trace ===\n").append(new String(data, StandardCharsets.UTF_8)).append('\n');
                }
            }
        } catch (IOException ex) {
            s.append("trace unavailable: ").append(ex).append('\n');
        }
        s.append("=== logcat left in the buffer ===\n");
        try {
            Process lc = new ProcessBuilder("logcat", "-d", "-v", "threadtime", "-t", "5000").redirectErrorStream(true).start();
            s.append(new String(lc.getInputStream().readNBytes(2<<20), StandardCharsets.UTF_8));
            lc.waitFor();
        } catch (IOException | InterruptedException ex) {
            s.append("logcat unavailable: ").append(ex).append('\n');
        }
        try { o.write(s.toString().getBytes(StandardCharsets.UTF_8)); } catch (IOException ignored) { }
        close(o);
    }

    // --- helpers

    /** A new file in Download/SpeedBreaker-logs; not IS_PENDING, so it stays visible if the process dies. */
    private static OutputStream create(Context ctx, String name) {
        try {
            ContentValues v = new ContentValues();
            v.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
            v.put(MediaStore.MediaColumns.MIME_TYPE, name.endsWith(".pb") ? "application/octet-stream"
                    : name.endsWith(".zip") ? "application/zip" : "text/plain");
            v.put(MediaStore.MediaColumns.RELATIVE_PATH, FOLDER);
            Uri uri = ctx.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
            if (uri != null) {
                OutputStream o = ctx.getContentResolver().openOutputStream(uri, "w");
                if (o != null) return o;
            }
        } catch (Exception e) {
            Log.w(TAG, "Downloads refused " + name + ": " + e);
        }
        try {
            File dir = new File(ctx.getExternalFilesDir(null), "logs");
            dir.mkdirs();
            return new FileOutputStream(new File(dir, name));
        } catch (IOException | NullPointerException e) {
            return null;
        }
    }

    private OutputStream bounded(OutputStream target) {
        if(target==null)return null;
        return new FilterOutputStream(target) {
            long written;
            @Override public void write(byte[] b,int off,int len)throws IOException {
                synchronized(DownloadsLog.this) {
                    int n=(int)Math.max(0,Math.min(len,Math.min(FILE_LIMIT-written,SESSION_LIMIT-bytesWritten)));
                    if(n>0){out.write(b,off,n);written+=n;bytesWritten+=n;}
                }
            }
            @Override public void write(int b)throws IOException {byte[] one={(byte)b};write(one,0,1);}
        };
    }

    private void write(String text) {
        synchronized (out) {
            try {
                byte[] bytes=text.getBytes(StandardCharsets.UTF_8);
                synchronized(this) {
                    int n=(int)Math.max(0,Math.min(bytes.length,Math.min(FILE_LIMIT-logTextBytes,SESSION_LIMIT-bytesWritten)));
                    if(n>0){out.write(bytes,0,n);logTextBytes+=n;bytesWritten+=n;out.flush();}
                }
            } catch(IOException ignored){}
        }
    }

    private static void close(OutputStream o) { synchronized (o) { try { o.close(); } catch (IOException ignored) { } } }
    private static void join(Thread t, long ms) { try { t.join(ms); } catch (InterruptedException ignored) { } }
    private static String stamp(Date d) { return new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.ROOT).format(d); }
    private static String read(String path) { try { return new String(Files.readAllBytes(new File(path).toPath()), StandardCharsets.UTF_8).trim(); } catch (Exception e) { return ""; } }
    private static long number(String s) { try { return Long.parseLong(s.trim()); } catch (NumberFormatException e) { return -1; } }

    private static String thermal(int s) {
        switch (s) {
            case PowerManager.THERMAL_STATUS_NONE: return "none";
            case PowerManager.THERMAL_STATUS_LIGHT: return "light";
            case PowerManager.THERMAL_STATUS_MODERATE: return "moderate";
            case PowerManager.THERMAL_STATUS_SEVERE: return "SEVERE";
            case PowerManager.THERMAL_STATUS_CRITICAL: return "CRITICAL";
            case PowerManager.THERMAL_STATUS_EMERGENCY: return "EMERGENCY";
            case PowerManager.THERMAL_STATUS_SHUTDOWN: return "SHUTDOWN";
            default: return String.valueOf(s);
        }
    }

    private static String reason(int r) {
        switch (r) {
            case ApplicationExitInfo.REASON_ANR: return "ANR";
            case ApplicationExitInfo.REASON_CRASH: return "Java crash";
            case ApplicationExitInfo.REASON_CRASH_NATIVE: return "native crash";
            case ApplicationExitInfo.REASON_LOW_MEMORY: return "low memory (LMK)";
            case ApplicationExitInfo.REASON_SIGNALED: return "signal";
            case ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE: return "excessive resource usage";
            case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE: return "initialization failure";
            case ApplicationExitInfo.REASON_DEPENDENCY_DIED: return "dependency died";
            default: return "other";
        }
    }
}
