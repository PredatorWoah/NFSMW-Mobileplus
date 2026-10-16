package io.github.nfsmwrecomp;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Debug;
import android.os.Environment;
import android.os.PowerManager;
import android.provider.MediaStore;
import android.util.Log;
import android.view.Display;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;

/**
 * El registro en Descargas: para mandar a quien diagnostica un movil que no
 * tiene a mano todo lo que paso en una partida o en una sonda, sin adb.
 *
 * Con el interruptor puesto (Ajustes.registroDescargas; por defecto, lo que
 * diga -Pnfsmw.registroDescargas=true al compilar), cada partida o sonda deja
 * en Download/NFSMW-logs/:
 *
 *   nfsmw_<fecha>_<tipo>.txt        cabecera (movil, SoC, nucleos, GPU,
 *                                   pantalla, ajustes y argumentos), el logcat
 *                                   entero de la app mientras dura (el SDK
 *                                   copia ahi cada linea suya, y el driver de
 *                                   Vulkan y SDL escriben ahi) y, cada 5 s, una
 *                                   linea "diag" con fps, temperatura, estado
 *                                   termico, memoria y frecuencia de cada nucleo
 *   nfsmw_<fecha>_<tipo>_juego.log  el registro del propio juego (log_file),
 *                                   copiado segun crece: el logcat puede perder
 *                                   lineas y en algunos moviles va capado
 *
 * y al abrir otra vez la app, si el proceso del juego no acabo bien (fallo,
 * fallo nativo, ANR, sin memoria), nfsmw_<fecha>_salida.txt con el motivo de
 * Android (ApplicationExitInfo), lo que quedaba en el logcat y, en un fallo
 * nativo, el tombstone (.pb, protobuf de Android).
 *
 * Todo se escribe y se vacia segun llega: si el proceso muere, lo escrito ya
 * esta en el fichero. Se escribe por MediaStore (sin permisos desde Android 10)
 * y, si el sistema no lo deja, en la carpeta de logs de la app.
 *
 * Cuesta algo de fps: el juego va con log_level=debug (ver Ajustes.nivelRegistro).
 */
final class RegistroDescargas {

    private static final String TAG = "nfsmw-diag";
    private static final String CARPETA = Environment.DIRECTORY_DOWNLOADS + "/NFSMW-logs";
    private static final String PREFS = "registro_descargas";
    private static final long CADA_MS = 1000;
    private static final int MUESTRA_CADA = 5;

    /** Lo que se esta escribiendo en este proceso, o null. */
    private static RegistroDescargas actual;

    private final Context ctx;
    private final OutputStream salida;
    private final File logJuego;
    private final OutputStream salidaJuego;
    private final MedidorFps fps;
    private Process logcat;
    private volatile boolean parar;
    private Thread hiloLogcat;
    private Thread hiloCopia;

    /** Los fps y ms del juego: GameActivity los saca del plugin de GPU. */
    interface MedidorFps {
        float[] medir();
    }

    private RegistroDescargas(Context ctx, OutputStream salida, File logJuego,
                              OutputStream salidaJuego, MedidorFps fps) {
        this.ctx = ctx;
        this.salida = salida;
        this.logJuego = logJuego;
        this.salidaJuego = salidaJuego;
        this.fps = fps;
    }

    static boolean activo(Context ctx) {
        return new Ajustes(ctx).registroDescargas();
    }

    /**
     * Empieza a escribir. Desde GameActivity.onCreate, ANTES de cargar las
     * librerias, para que salga tambien lo que pase al cargarlas. tipo: "partida"
     * o "sonda".
     */
    static synchronized void arrancar(Context ctx, String tipo, String[] argumentos,
                                      MedidorFps fps) {
        if (actual != null || !activo(ctx)) {
            return;
        }
        String base = "nfsmw_" + fecha() + "_" + tipo;
        OutputStream salida = crear(ctx, base + ".txt");
        if (salida == null) {
            return;
        }
        File logJuego = null;
        for (String a : argumentos) {
            if (a.startsWith("--log_file=")) {
                File f = new File(a.substring("--log_file=".length()));
                if (!f.getPath().equals("/dev/null")) {
                    logJuego = f;
                }
            }
        }
        OutputStream salidaJuego = logJuego != null ? crear(ctx, base + "_juego.log") : null;
        RegistroDescargas r = new RegistroDescargas(ctx.getApplicationContext(), salida, logJuego,
                salidaJuego, fps);
        r.escribir(cabecera(ctx, tipo, argumentos));
        r.empezar();
        actual = r;
    }

    /** Lo ultimo que quede por copiar, y cerrar. Desde GameActivity.onDestroy. */
    static synchronized void parar() {
        RegistroDescargas r = actual;
        actual = null;
        if (r != null) {
            r.terminar();
        }
    }

    // ------------------------------------------------------------------
    //  Mientras dura la partida
    // ------------------------------------------------------------------

    private void empezar() {
        // Un fallo de Java mata el proceso antes de que el logcat llegue al
        // fichero: se escribe aqui mismo, y despues lo de siempre.
        Thread.UncaughtExceptionHandler previo = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            StringWriter sw = new StringWriter();
            e.printStackTrace(new PrintWriter(sw));
            escribir("\n=== FALLO DE JAVA en el hilo '" + t.getName() + "' ===\n" + sw + "\n");
            if (previo != null) {
                previo.uncaughtException(t, e);
            }
        });

        PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            pm.addThermalStatusListener(ctx.getMainExecutor(),
                    estado -> Log.w(TAG, "estado termico -> " + termico(estado)));
        }

        // Solo lo de esta app (Android no deja leer el de otras) y desde ahora:
        // sin -T saldria todo lo que quede en el bufer de sesiones anteriores.
        long ahora = System.currentTimeMillis();
        String desde = String.format(Locale.ROOT, "%d.%03d", ahora / 1000, ahora % 1000);
        hiloLogcat = new Thread(() -> {
            try {
                logcat = new ProcessBuilder("logcat", "-v", "threadtime", "-T", desde)
                        .redirectErrorStream(true).start();
                try (BufferedReader in = new BufferedReader(new InputStreamReader(
                        logcat.getInputStream(), StandardCharsets.UTF_8), 1 << 16)) {
                    StringBuilder bloque = new StringBuilder(1 << 14);
                    String linea;
                    while (!parar && (linea = in.readLine()) != null) {
                        bloque.append(linea).append('\n');
                        // De golpe lo que ya haya llegado, y se vacia al quedarse
                        // sin nada pendiente: pocas escrituras y nada atrasado.
                        if (!in.ready() || bloque.length() > (1 << 14)) {
                            escribir(bloque.toString());
                            bloque.setLength(0);
                        }
                    }
                    if (bloque.length() > 0) {
                        escribir(bloque.toString());
                    }
                }
            } catch (IOException e) {
                escribir("\n[registro] logcat no disponible: " + e + "\n");
            }
        }, "NFS registro logcat");
        hiloLogcat.setDaemon(true);
        hiloLogcat.start();

        hiloCopia = new Thread(() -> {
            long copiado = 0;
            int vuelta = 0;
            while (!parar) {
                copiado = copiarLogJuego(copiado);
                if (++vuelta % MUESTRA_CADA == 0) {
                    Log.i(TAG, muestra());
                }
                try {
                    Thread.sleep(CADA_MS);
                } catch (InterruptedException e) {
                    break;
                }
            }
            copiarLogJuego(copiado);
        }, "NFS registro copia");
        hiloCopia.setDaemon(true);
        hiloCopia.start();
    }

    private void terminar() {
        Log.i(TAG, "fin de la sesion: " + muestra());
        parar = true;
        if (hiloCopia != null) {
            hiloCopia.interrupt();
            try {
                hiloCopia.join(2000);
            } catch (InterruptedException ignored) {
            }
        }
        // Lo que el logcat aun no haya entregado (las ultimas lineas) se pierde
        // al matarlo; da igual: esta sesion acaba bien.
        if (logcat != null) {
            logcat.destroy();
        }
        escribir("\n=== fin " + fecha() + " ===\n");
        cerrar(salida);
        if (salidaJuego != null) {
            cerrar(salidaJuego);
        }
    }

    /**
     * Lo que haya crecido el log del juego desde la ultima vez. El SDK lo
     * reescribe al arrancar: si encoge, se empieza de cero.
     */
    private long copiarLogJuego(long desde) {
        if (logJuego == null || salidaJuego == null || !logJuego.isFile()) {
            return desde;
        }
        long largo = logJuego.length();
        if (largo < desde) {
            desde = 0;
        }
        if (largo == desde) {
            return desde;
        }
        try (RandomAccessFile f = new RandomAccessFile(logJuego, "r")) {
            f.seek(desde);
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = f.read(buf)) > 0) {
                synchronized (salidaJuego) {
                    salidaJuego.write(buf, 0, n);
                }
                desde += n;
            }
            synchronized (salidaJuego) {
                salidaJuego.flush();
            }
        } catch (IOException e) {
            Log.w(TAG, "no se pudo copiar " + logJuego + ": " + e);
        }
        return desde;
    }

    /** Una linea con lo que cambia durante la partida. */
    private String muestra() {
        StringBuilder s = new StringBuilder("diag");
        float[] m = fps != null ? fps.medir() : null;
        if (m != null && m[0] >= 0) {
            s.append(String.format(Locale.ROOT, " fps=%.1f ms=%.1f", m[0], m[1]));
        } else {
            s.append(" fps=--");
        }
        PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
        if (pm != null) {
            s.append(" termico=").append(termico(pm.getCurrentThermalStatus()));
            if (Build.VERSION.SDK_INT >= 30) {
                float margen = pm.getThermalHeadroom(0);
                if (!Float.isNaN(margen)) {
                    s.append(String.format(Locale.ROOT, " margen=%.2f", margen));
                }
            }
        }
        Intent bateria = ctx.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (bateria != null) {
            s.append(String.format(Locale.ROOT, " bateria=%d%% %.1fC",
                    bateria.getIntExtra(BatteryManager.EXTRA_LEVEL, -1),
                    bateria.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) / 10f));
        }
        ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            s.append(" ram_libre=").append(mi.availMem >> 20).append("MB");
            if (mi.lowMemory) {
                s.append(" (POCA)");
            }
        }
        s.append(" rss=").append(rssMb()).append("MB");
        s.append(" heap_nativo=").append(Debug.getNativeHeapAllocatedSize() >> 20).append("MB");
        s.append(" mhz=");
        int nucleos = Runtime.getRuntime().availableProcessors();
        for (int c = 0; c < nucleos; c++) {
            String f = leer("/sys/devices/system/cpu/cpu" + c + "/cpufreq/scaling_cur_freq");
            long khz = numero(f);
            s.append(c == 0 ? "" : ",").append(khz < 0 ? "?" : String.valueOf(khz / 1000));
        }
        return s.toString();
    }

    private static long rssMb() {
        for (String l : leer("/proc/self/status").split("\n")) {
            if (l.startsWith("VmRSS:")) {
                long kb = numero(l.replaceAll("[^0-9]", ""));
                return kb < 0 ? -1 : kb >> 10;
            }
        }
        return -1;
    }

    private void escribir(String texto) {
        synchronized (salida) {
            try {
                salida.write(texto.getBytes(StandardCharsets.UTF_8));
                salida.flush();
            } catch (IOException ignored) {
                // Descargas llena o el fichero borrado a mano: no hay a quien avisar.
            }
        }
    }

    // ------------------------------------------------------------------
    //  Lo que se sabe al empezar
    // ------------------------------------------------------------------

    private static String cabecera(Context ctx, String tipo, String[] argumentos) {
        StringBuilder s = new StringBuilder();
        s.append("=== NFSMW ").append(tipo).append(" ").append(fecha()).append(" ===\n");
        s.append("app: ").append(BuildConfig.VERSION_NAME).append(" (").append(BuildConfig.VERSION_CODE)
                .append(") motor=").append(BuildConfig.MOTOR).append(" con_juego=")
                .append(BuildConfig.CON_JUEGO).append(" depuracion=").append(BuildConfig.DEBUG).append('\n');
        s.append("ediciones: ").append(BuildConfig.EDICIONES).append('\n');
        s.append("movil: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append(" (").append(Build.DEVICE).append(", ").append(Build.HARDWARE).append(")\n");
        if (Build.VERSION.SDK_INT >= 31) {
            s.append("soc: ").append(Build.SOC_MANUFACTURER).append(' ').append(Build.SOC_MODEL).append('\n');
        }
        s.append("android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT)
                .append(") parche ").append(Build.VERSION.SECURITY_PATCH).append('\n');
        s.append("sistema: ").append(Build.FINGERPRINT).append('\n');
        s.append("gpu: ").append(Gpu.nombre(ctx)).append(" familia=").append(Gpu.familia(ctx))
                .append(" debil=").append(Gpu.debil(ctx)).append('\n');

        ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            s.append("ram: ").append(mi.totalMem >> 20).append(" MB, libre ").append(mi.availMem >> 20)
                    .append(" MB, umbral ").append(mi.threshold >> 20).append(" MB\n");
        }

        // Lo que afinidad.cpp usa para decidir los nucleos prime.
        int nucleos = Runtime.getRuntime().availableProcessors();
        s.append("nucleos: ").append(nucleos).append('\n');
        for (int c = 0; c < nucleos; c++) {
            String d = "/sys/devices/system/cpu/cpu" + c + "/";
            s.append("  cpu").append(c)
                    .append(" capacidad=").append(leer(d + "cpu_capacity"))
                    .append(" max_khz=").append(leer(d + "cpufreq/cpuinfo_max_freq"))
                    .append(" gobernador=").append(leer(d + "cpufreq/scaling_governor"))
                    .append(" parte=").append(parteCpu(c)).append('\n');
        }

        Display pantalla = ctx.getSystemService(android.hardware.display.DisplayManager.class)
                .getDisplay(Display.DEFAULT_DISPLAY);
        if (pantalla != null) {
            s.append("pantalla:");
            for (Display.Mode m : pantalla.getSupportedModes()) {
                s.append(String.format(Locale.ROOT, " %dx%d@%.0f", m.getPhysicalWidth(),
                        m.getPhysicalHeight(), m.getRefreshRate()));
            }
            s.append(String.format(Locale.ROOT, " (ahora %.0f Hz)\n", pantalla.getRefreshRate()));
        }

        s.append("ajustes:\n");
        Map<String, ?> todos = new TreeMap<>(ctx.getSharedPreferences(Ajustes.PREFS,
                Context.MODE_PRIVATE).getAll());
        for (Map.Entry<String, ?> e : todos.entrySet()) {
            // La URI de la ISO lleva el nombre del fichero: no hace falta para nada.
            if (e.getKey().startsWith("iso")) {
                continue;
            }
            s.append("  ").append(e.getKey()).append('=').append(e.getValue()).append('\n');
        }
        s.append("argumentos:\n");
        for (String a : argumentos) {
            s.append("  ").append(a.startsWith("--game_data_root=") ? "--game_data_root=<ISO>" : a)
                    .append('\n');
        }
        s.append("=== logcat ===\n");
        return s.toString();
    }

    /** El "CPU part" de /proc/cpuinfo del nucleo c: dice que modelo de Arm es. */
    private static String parteCpu(int c) {
        boolean suyo = false;
        for (String l : leer("/proc/cpuinfo").split("\n")) {
            if (l.startsWith("processor")) {
                suyo = l.replaceAll("[^0-9]", "").equals(String.valueOf(c));
            } else if (suyo && l.startsWith("CPU part")) {
                return l.substring(l.indexOf(':') + 1).trim();
            }
        }
        return "?";
    }

    // ------------------------------------------------------------------
    //  Al abrir la app: como acabo la partida anterior
    // ------------------------------------------------------------------

    /**
     * Desde la pantalla de inicio (proceso principal). Las salidas del juego
     * desde la ultima vez que se miro que no fueron un cierre normal (al
     * cerrar la partida, GameActivity mata su propio proceso: SIGKILL, 9).
     */
    static void informeSalidas(Context ctx) {
        if (Build.VERSION.SDK_INT < 30 || !activo(ctx)) {
            return;
        }
        ActivityManager am = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
        if (am == null) {
            return;
        }
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        long visto = p.getLong("salida_vista", 0);
        List<ApplicationExitInfo> salidas = am.getHistoricalProcessExitReasons(null, 0, 16);
        long ultima = visto;
        for (ApplicationExitInfo e : salidas) {
            if (e.getTimestamp() <= visto) {
                continue;
            }
            ultima = Math.max(ultima, e.getTimestamp());
            int motivo = e.getReason();
            boolean normal = motivo == ApplicationExitInfo.REASON_EXIT_SELF
                    || motivo == ApplicationExitInfo.REASON_USER_REQUESTED
                    || (motivo == ApplicationExitInfo.REASON_SIGNALED && e.getStatus() == 9);
            if (normal) {
                continue;
            }
            escribirSalida(ctx, e);
        }
        p.edit().putLong("salida_vista", ultima).apply();
    }

    private static void escribirSalida(Context ctx, ApplicationExitInfo e) {
        String base = "nfsmw_" + new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.ROOT)
                .format(new Date(e.getTimestamp())) + "_salida";
        OutputStream o = crear(ctx, base + ".txt");
        if (o == null) {
            return;
        }
        StringBuilder s = new StringBuilder();
        s.append("=== salida del proceso ").append(e.getProcessName()).append(" ===\n");
        s.append("cuando: ").append(new Date(e.getTimestamp())).append('\n');
        s.append("motivo: ").append(motivo(e.getReason())).append(" (").append(e.getReason())
                .append(") estado=").append(e.getStatus()).append(" importancia=")
                .append(e.getImportance()).append('\n');
        s.append("descripcion: ").append(e.getDescription()).append('\n');
        s.append("pss=").append(e.getPss() >> 10).append("MB rss=").append(e.getRss() >> 10).append("MB\n");
        boolean tombstone = false;
        try (InputStream traza = e.getTraceInputStream()) {
            if (traza != null) {
                byte[] datos = leerTodo(traza);
                // En un ANR es texto; en un fallo nativo (Android 12+), un
                // tombstone en protobuf: aparte, tal cual.
                if (e.getReason() == ApplicationExitInfo.REASON_CRASH_NATIVE) {
                    OutputStream t = crear(ctx, base + "_tombstone.pb");
                    if (t != null) {
                        t.write(datos);
                        cerrar(t);
                        tombstone = true;
                    }
                } else {
                    s.append("=== traza ===\n").append(new String(datos, StandardCharsets.UTF_8)).append('\n');
                }
            }
        } catch (IOException ex) {
            s.append("traza no disponible: ").append(ex).append('\n');
        }
        if (tombstone) {
            s.append("tombstone: ").append(base).append("_tombstone.pb\n");
        }
        // Lo que quede en el bufer del logcat de esta app: con suerte, el
        // backtrace de debuggerd y las ultimas lineas del juego.
        s.append("=== logcat que quedaba ===\n");
        try {
            Process lc = new ProcessBuilder("logcat", "-d", "-v", "threadtime", "-t", "5000")
                    .redirectErrorStream(true).start();
            s.append(new String(leerTodo(lc.getInputStream()), StandardCharsets.UTF_8));
            lc.waitFor();
        } catch (IOException | InterruptedException ex) {
            s.append("logcat no disponible: ").append(ex).append('\n');
        }
        try {
            o.write(s.toString().getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
        }
        cerrar(o);
    }

    /** El informe de la sonda de Vulkan, la primera vez que se ve uno nuevo. */
    static void copiarInformeSonda(Context ctx) {
        if (!activo(ctx)) {
            return;
        }
        File informe = Ajustes.informeSonda(ctx);
        if (!informe.isFile()) {
            return;
        }
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        if (p.getLong("sonda_copiada", 0) == informe.lastModified()) {
            return;
        }
        OutputStream o = crear(ctx, "nfsmw_" + fecha() + "_sonda_vulkan.txt");
        if (o == null) {
            return;
        }
        try {
            o.write(Files.readAllBytes(informe.toPath()));
        } catch (IOException ignored) {
        }
        cerrar(o);
        p.edit().putLong("sonda_copiada", informe.lastModified()).apply();
    }

    // ------------------------------------------------------------------
    //  Utilidades
    // ------------------------------------------------------------------

    /**
     * Un fichero nuevo en Download/NFSMW-logs. Sin IS_PENDING: se tiene que ver
     * (y quedarse) aunque el proceso muera a medias, que es justo para lo que
     * sirve. Si MediaStore no deja, en la carpeta de logs de la app.
     */
    private static OutputStream crear(Context ctx, String nombre) {
        try {
            ContentValues v = new ContentValues();
            v.put(MediaStore.MediaColumns.DISPLAY_NAME, nombre);
            v.put(MediaStore.MediaColumns.MIME_TYPE,
                    nombre.endsWith(".pb") ? "application/octet-stream" : "text/plain");
            v.put(MediaStore.MediaColumns.RELATIVE_PATH, CARPETA);
            Uri uri = ctx.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);
            if (uri != null) {
                OutputStream o = ctx.getContentResolver().openOutputStream(uri, "w");
                if (o != null) {
                    return o;
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Descargas no disponible para " + nombre + ": " + e);
        }
        try {
            return new FileOutputStream(new File(Ajustes.carpetaLogs(ctx), nombre));
        } catch (IOException e) {
            Log.w(TAG, "tampoco en la carpeta de logs: " + e);
            return null;
        }
    }

    private static void cerrar(OutputStream o) {
        synchronized (o) {
            try {
                o.close();
            } catch (IOException ignored) {
            }
        }
    }

    private static byte[] leerTodo(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream b = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[1 << 16];
        int n;
        while ((n = in.read(buf)) > 0) {
            b.write(buf, 0, n);
        }
        return b.toByteArray();
    }

    /** Un fichero pequeno de /proc o /sys, sin el salto final; "" si no se puede. */
    private static String leer(String ruta) {
        try {
            return new String(Files.readAllBytes(new File(ruta).toPath()), StandardCharsets.UTF_8).trim();
        } catch (Exception e) {
            return "";
        }
    }

    /** Un numero de /proc o /sys, o -1 si no lo es (nucleo apagado, kernel raro). */
    private static long numero(String texto) {
        try {
            return Long.parseLong(texto.trim());
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    private static String fecha() {
        return new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.ROOT).format(new Date());
    }

    private static String termico(int estado) {
        switch (estado) {
            case PowerManager.THERMAL_STATUS_NONE: return "ninguno";
            case PowerManager.THERMAL_STATUS_LIGHT: return "ligero";
            case PowerManager.THERMAL_STATUS_MODERATE: return "moderado";
            case PowerManager.THERMAL_STATUS_SEVERE: return "SEVERO";
            case PowerManager.THERMAL_STATUS_CRITICAL: return "CRITICO";
            case PowerManager.THERMAL_STATUS_EMERGENCY: return "EMERGENCIA";
            case PowerManager.THERMAL_STATUS_SHUTDOWN: return "APAGADO";
            default: return String.valueOf(estado);
        }
    }

    private static String motivo(int m) {
        switch (m) {
            case ApplicationExitInfo.REASON_ANR: return "ANR";
            case ApplicationExitInfo.REASON_CRASH: return "fallo de Java";
            case ApplicationExitInfo.REASON_CRASH_NATIVE: return "fallo nativo";
            case ApplicationExitInfo.REASON_LOW_MEMORY: return "sin memoria (LMK)";
            case ApplicationExitInfo.REASON_SIGNALED: return "senal";
            case ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE: return "demasiados recursos";
            case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE: return "fallo al iniciar";
            case ApplicationExitInfo.REASON_DEPENDENCY_DIED: return "murio una dependencia";
            case ApplicationExitInfo.REASON_PERMISSION_CHANGE: return "cambio de permisos";
            case ApplicationExitInfo.REASON_OTHER: return "otro";
            default: return "desconocido";
        }
    }
}
