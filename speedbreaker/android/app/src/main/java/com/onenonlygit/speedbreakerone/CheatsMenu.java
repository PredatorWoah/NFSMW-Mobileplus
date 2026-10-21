// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Button;
import android.text.InputType;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

/**
 * The CHEATS menu (top right in the game). Two kinds:
 *
 *   Bonus codes   the game's own title-screen button codes, typed through the
 *                 virtual gamepad. Enter them on the title screen ("Press
 *                 Start"); they unlock for the profile, as on a 360.
 *   Start-up      data patches the runtime applies when the game starts
 *                 (runtime/game/cheats.cpp), chosen here, saved to
 *                 <user folder>/cheats.txt; Save & Restart starts the game
 *                 again with them.
 *
 *   Money         a number finder (runtime/platform/android/cheat_finder.cpp):
 *                 search the game's memory for the money shown, change it in
 *                 the game, search again, then set the address that is left.
 *
 * Only what has a source is offered. Unlimited nitro and SpeedBreaker, all
 * cars and gold medals need their Xbox 360 addresses found first
 * (docs/speedbreaker.md).
 */
final class CheatsMenu {
    private static final String TAG = "SpeedBreakerOne";
    private static final int LB = 1 << 9, RB = 1 << 10, UP = 1 << 11, DOWN = 1 << 12, LEFT = 1 << 13, RIGHT = 1 << 14;
    // A step: buttons, and the two triggers (the Cayman code's L/R may mean them).
    private static final int LT = -1, RT = -2;
    private static final long PRESS_MS = 150, GAP_MS = 150;

    private static final class Code {
        final String name, sequence; final int[][] variants;
        Code(String name, String sequence, int[]... variants) { this.name = name; this.sequence = sequence; this.variants = variants; }
    }

    // The 360 version's title-screen codes (gamerevolution.com, cheatcc.com,
    // cheathappens.com; Castrol and Burger King are swapped in one listing).
    private static final Code[] CODES = {
        new Code("Junkman engine marker (One Stop Shop)", "Up Up Down Down Left Right Up Down",
                new int[] {UP, UP, DOWN, DOWN, LEFT, RIGHT, UP, DOWN}),
        new Code("Burger King challenge", "Up Down Up Down Left Right Left Right",
                new int[] {UP, DOWN, UP, DOWN, LEFT, RIGHT, LEFT, RIGHT}),
        new Code("Castrol Syntec Ford GT", "Left Right Left Right Up Down Up Down",
                new int[] {LEFT, RIGHT, LEFT, RIGHT, UP, DOWN, UP, DOWN}),
        // Listed as "L, R, R, R": bumpers on some lists, triggers on others.
        // Both are sent; a sequence the game does not know does nothing.
        new Code("Porsche Cayman", "L R R R Right Left Right Down",
                new int[] {LB, RB, RB, RB, RIGHT, LEFT, RIGHT, DOWN},
                new int[] {LT, RT, RT, RT, RIGHT, LEFT, RIGHT, DOWN}),
    };

    // Start-up cheats: id (runtime/game/cheats.cpp), label, what it does.
    private static final String[][] STARTUP = {
        {"black_edition", "Black Edition content", "The Collector's Edition bonus cars and races (Xenia's patch for this disc)."},
    };

    // The number finder, in libmain (cheat_finder.cpp): match count, or the
    // number of addresses written.
    private static native int nativeFindFirst(int value);
    private static native int nativeFindNext(int value);
    private static native int nativeSetMatches(int value, int limit);
    private static final int SET_LIMIT = 8;
    private static volatile int finderMatches = -1;  // -1: no search yet (kept while the process lives)

    private final Activity activity;
    private final TouchGamepadView pad;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean typing;

    CheatsMenu(Activity activity, TouchGamepadView pad) { this.activity = activity; this.pad = pad; }

    void show() {
        float dp = activity.getResources().getDisplayMetrics().density;
        LinearLayout list = new LinearLayout(activity);
        list.setOrientation(LinearLayout.VERTICAL);
        int pad16 = (int) (16 * dp);
        list.setPadding(pad16, pad16 / 2, pad16, pad16);

        list.addView(heading("Bonus codes"));
        list.addView(note("Go to the title screen (\"Press Start\"), then tap a code. It is typed for you; don't touch the screen until it's done."));
        AlertDialog[] dialog = new AlertDialog[1];
        for (Code code : CODES) {
            Button b = new Button(activity);
            b.setText(code.name + "\n" + code.sequence);
            b.setAllCaps(false);
            b.setOnClickListener(v -> { dialog[0].dismiss(); type(new Code[] {code}); });
            list.addView(b);
        }
        Button all = new Button(activity);
        all.setText("Enter all four");
        all.setOnClickListener(v -> { dialog[0].dismiss(); type(CODES); });
        list.addView(all);

        list.addView(heading("At game start"));
        List<String> enabled = readEnabled();
        List<CheckBox> boxes = new ArrayList<>();
        for (String[] c : STARTUP) {
            CheckBox box = new CheckBox(activity);
            box.setText(c[1] + "\n" + c[2]);
            box.setChecked(enabled.contains(c[0]));
            box.setTag(c[0]);
            boxes.add(box);
            list.addView(box);
        }
        Button restart = new Button(activity);
        restart.setText("Save & Restart");
        restart.setOnClickListener(v -> new AlertDialog.Builder(activity)
                .setMessage("Save these choices and restart the game? Unsaved race progress will be lost.")
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Restart", (d, w) -> { save(boxes); restartGame(); })
                .show());
        list.addView(restart);

        list.addView(heading("Money"));
        list.addView(note("Finds where the game keeps your money, then sets it. Bounty works the same way."));
        Button money = new Button(activity);
        money.setText("Money finder");
        money.setOnClickListener(v -> { dialog[0].dismiss(); moneyFinder(); });
        list.addView(money);

        list.addView(heading("Not available yet"));
        list.addView(note("Unlimited nitro, unlimited SpeedBreaker, all cars and gold medals: their places in the Xbox 360 game still have to be found. They will appear here when they work."));

        ScrollView scroll = new ScrollView(activity);
        scroll.addView(list);
        dialog[0] = new AlertDialog.Builder(activity).setTitle("Cheats").setView(scroll)
                .setNegativeButton("Close", null).create();
        dialog[0].setOnDismissListener(d -> { });
        dialog[0].show();
    }

    private TextView heading(String text) {
        TextView t = new TextView(activity);
        t.setText(text);
        t.setTextSize(18);
        t.setPadding(0, (int) (12 * activity.getResources().getDisplayMetrics().density), 0, 0);
        return t;
    }

    private TextView note(String text) {
        TextView t = new TextView(activity);
        t.setText(text);
        t.setAlpha(0.8f);
        return t;
    }

    // --- The money finder: search, change the money in the game, search again.

    private void moneyFinder() {
        float dp = activity.getResources().getDisplayMetrics().density;
        LinearLayout box = new LinearLayout(activity);
        box.setOrientation(LinearLayout.VERTICAL);
        int pad16 = (int) (16 * dp);
        box.setPadding(pad16, pad16 / 2, pad16, 0);
        TextView status = note(finderStatus());
        EditText number = new EditText(activity);
        number.setInputType(InputType.TYPE_CLASS_NUMBER);
        number.setHint("Money shown in the game, e.g. 12500");
        box.addView(status);
        box.addView(number);
        AlertDialog d = new AlertDialog.Builder(activity).setTitle("Money finder").setView(box)
                .setPositiveButton(finderMatches < 0 ? "Search" : "Search again", null)
                .setNeutralButton("Start over", null)
                .setNegativeButton("Close", null).create();
        d.setOnShowListener(x -> {
            Button search = d.getButton(AlertDialog.BUTTON_POSITIVE), over = d.getButton(AlertDialog.BUTTON_NEUTRAL);
            Runnable refresh = () -> {
                status.setText(finderStatus());
                search.setText(finderMatches < 0 ? "Search" : finderMatches <= SET_LIMIT && finderMatches > 0 ? "Set money" : "Search again");
                number.setHint(finderMatches > 0 && finderMatches <= SET_LIMIT ? "Money you want, e.g. 9999999" : "Money shown in the game, e.g. 12500");
                number.setText("");
            };
            refresh.run();
            over.setOnClickListener(v -> { finderMatches = -1; refresh.run(); });
            search.setOnClickListener(v -> {
                Integer value = parse(number.getText().toString());
                if (value == null) { Toast.makeText(activity, "Type a whole number", Toast.LENGTH_SHORT).show(); return; }
                boolean setting = finderMatches > 0 && finderMatches <= SET_LIMIT;
                boolean first = finderMatches < 0;
                search.setEnabled(false);
                status.setText(setting ? "Setting…" : "Searching the game's memory…");
                new Thread(() -> {
                    String done;
                    try {
                        if (setting) {
                            int n = nativeSetMatches(value, SET_LIMIT);
                            done = n > 0 ? "Money set at " + n + " place" + (n > 1 ? "s" : "") + ". It shows the next time the game redraws it (open a menu); it is kept when the game saves." : "Nothing set.";
                        } else {
                            finderMatches = first ? nativeFindFirst(value) : nativeFindNext(value);
                            done = null;
                        }
                    } catch (UnsatisfiedLinkError e) {
                        done = "The finder is not in this build.";
                    }
                    final String message = done;
                    activity.runOnUiThread(() -> {
                        search.setEnabled(true);
                        refresh.run();
                        if (message != null) Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
                    });
                }, "money-finder").start();
            });
        });
        d.show();
    }

    private static String finderStatus() {
        if (finderMatches < 0) return "1. Type the money the game shows now and tap Search.";
        if (finderMatches == 0) return "No match. The number must be exactly what the game shows now. Tap Start over.";
        if (finderMatches <= SET_LIMIT) return "Found (" + finderMatches + " place" + (finderMatches > 1 ? "s" : "") + "). Type the money you want and tap Set money.";
        return finderMatches + " places still match. 2. Change your money in the game (buy a part, win a race), come back, type the new amount and tap Search again.";
    }

    private static Integer parse(String text) {
        String digits = text.replaceAll("[^0-9]", "");
        if (digits.isEmpty() || digits.length() > 10) return null;
        long v = Long.parseLong(digits);
        return v > Integer.MAX_VALUE ? null : (int) v;
    }

    // --- Typing the codes: one press and release per step, on the UI thread.

    private void type(Code[] codes) {
        if (typing) return;
        typing = true;
        if (pad != null) pad.reset();
        List<int[]> steps = new ArrayList<>();
        for (Code c : codes) for (int[] variant : c.variants) { steps.add(variant); steps.add(null); }  // null: a pause
        Toast.makeText(activity, "Typing code" + (codes.length > 1 ? "s" : "") + "… don't touch the screen", Toast.LENGTH_SHORT).show();
        Log.i(TAG, "[cheats] typing " + codes.length + " title-screen code(s)");
        long t = 300;
        for (int[] seq : steps) {
            if (seq == null) { t += 600; continue; }
            for (int button : seq) {
                final int b = button;
                handler.postDelayed(() -> press(b), t);
                handler.postDelayed(() -> TouchGamepadView.inject(0, 0, 0), t + PRESS_MS);
                t += PRESS_MS + GAP_MS;
            }
        }
        handler.postDelayed(() -> {
            typing = false;
            if (pad != null) pad.reset();
            Toast.makeText(activity, "Done. A code that worked shows its unlock (or a sound) in the game.", Toast.LENGTH_LONG).show();
        }, t);
    }

    private static void press(int button) {
        if (button == LT) TouchGamepadView.inject(0, 1, 0);
        else if (button == RT) TouchGamepadView.inject(0, 0, 1);
        else TouchGamepadView.inject(button, 0, 0);
    }

    // --- Start-up cheats: <files>/data/speedbreaker/cheats.txt (user/paths.h).

    private File cheatsFile() {
        File data = new File(activity.getFilesDir(), "data");
        File old = new File(data, "nfsmw-recomp"), current = new File(data, "speedbreaker");
        File user = !current.isDirectory() && old.isDirectory() ? old : current;
        return new File(user, "cheats.txt");
    }

    private List<String> readEnabled() {
        List<String> on = new ArrayList<>();
        try {
            for (String line : Files.readAllLines(cheatsFile().toPath(), StandardCharsets.UTF_8))
                if (line.trim().endsWith("=1")) on.add(line.trim().substring(0, line.trim().length() - 2));
        } catch (IOException ignored) { }
        return on;
    }

    private void save(List<CheckBox> boxes) {
        StringBuilder s = new StringBuilder("# Written by the CHEATS menu; read at game start (runtime/game/cheats.cpp).\n");
        for (CheckBox b : boxes) s.append(b.getTag()).append('=').append(b.isChecked() ? 1 : 0).append('\n');
        File f = cheatsFile();
        f.getParentFile().mkdirs();
        try (FileOutputStream out = new FileOutputStream(f)) {
            out.write(s.toString().getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        } catch (IOException e) {
            Toast.makeText(activity, "Could not save: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
        Log.i(TAG, "[cheats] saved: " + s.toString().replace('\n', ' '));
    }

    // The launcher restarts the game process (its "Restart game" path), from
    // outside it: this process is the one that goes.
    private void restartGame() {
        Intent intent = new Intent(activity, LauncherActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(LauncherActivity.EXTRA_RESTART, true);
        activity.startActivity(intent);
    }
}
