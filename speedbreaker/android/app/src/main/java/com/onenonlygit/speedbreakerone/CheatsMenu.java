// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Title-screen codes with durable saved selections. Guarded optional USA hooks; no arbitrary-memory writes. */
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

    private final Activity activity;
    private final TouchGamepadView pad;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean typing;
    private AlertDialog shown;

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

        list.addView(heading("Saved code selection"));
        list.addView(note("Save which title-screen codes you want. After restarting, go to Press Start and use Enter selected. Codes are not automatically sent into menus or a loaded save."));
        List<String> enabled = readEnabled();
        List<CheckBox> boxes = new ArrayList<>();
        for (int i=0;i<CODES.length;i++) {
            CheckBox box = new CheckBox(activity);
            box.setText(CODES[i].name);
            box.setTag("code_"+i);
            box.setChecked(enabled.contains("code_"+i));
            boxes.add(box);list.addView(box);
        }
        list.addView(note("Published bonus codes are not yet verified on this USA build. Turning a saved code off stops future entry; it does not reverse unlocks already saved by the game."));
        list.addView(heading("Experimental USA startup hook"));
        CheckBox cash=new CheckBox(activity);
        cash.setText("Career payout cash top-up to 100,000,000 (experimental, OFF by default)");
        cash.setTag("career_cash_payout");
        cash.setChecked(read(new File(cheatsFile().getParentFile(),"cheats.txt")).contains("career_cash_payout"));
        boxes.add(cash);list.addView(cash);
        list.addView(note("Applies after Save & Restart, on the next career race payout. This is not unlimited spending. The USA instruction sequence is checked, but device behavior is unverified. The game may save the resulting balance; disabling this stops future top-ups, without restoring an earlier balance."));
        CheckBox edition=new CheckBox(activity);
        edition.setText("Black Edition bonus cars and events");
        edition.setTag("black_edition");
        edition.setChecked(read(new File(cheatsFile().getParentFile(),"cheats.txt")).contains("black_edition"));
        boxes.add(edition);list.addView(edition);
        list.addView(note("Uses the USA game's bonus-content flag, not a different executable. You reported this working. Apply with Save & Restart; disable and restart to stop applying the flag. Unlocks already saved by the game may remain."));
        List<String> startup=read(new File(cheatsFile().getParentFile(),"cheats.txt"));
        list.addView(heading("Unlimited meters"));
        for (String[] m : new String[][] {
                {"infinite_nitro", "Unlimited nitro"},
                {"infinite_speedbreaker", "Unlimited SpeedBreaker"}}) {
            CheckBox box=new CheckBox(activity);
            box.setText(m[1]);
            box.setTag(m[0]);
            box.setChecked(startup.contains(m[0]));
            boxes.add(box);list.addView(box);
        }
        list.addView(note("The USA game's own debug flags: an active meter keeps its charge instead of draining. It must have charge to start, and a car without NOS gets none. Apply with Save & Restart; turn off and restart to drain normally again."));
        CheckBox twice=new CheckBox(activity);
        twice.setText("2x nitro power (your car only)");
        twice.setTag("nitro_2x");
        twice.setChecked(startup.contains("nitro_2x"));
        boxes.add(twice);list.addView(twice);
        list.addView(note("While your nitro fires, its push is doubled. Rivals, cops and traffic keep their normal nitro, and nothing changes when it's off. Apply with Save & Restart."));
        list.addView(heading("Career"));
        for (String[] m : new String[][] {
                {"unlimited_money", "Unlimited money", "Your cash is topped back up to $100,000,000 every frame while the game runs (more is kept). Buy anything; the game saves the balance as usual."},
                {"unlock_all_cars", "Unlock all cars", "Every car in the game's lists answers \"unlocked\", bonus cars included, and the game's own unlock-all flag is set. Dealer stock and prices stay the game's."},
                {"challenge_stars", "Challenge Series: all completed", "The Challenge Series screen shows every event completed, and the series reward check passes. No event record is rewritten; once the game grants the reward, it keeps it."},
                {"sure_pink_slip", "Sure-shot pink slip", "After you beat a Blacklist rival, the first marker you flip is always the rival's pink slip (the hidden one is swapped under your pick, or made if none of the six is one). Your second pick is a normal marker."},
                {"plenty_markers", "Plenty of upgrade markers", "Keeps at least two of every part marker (performance and visual) in your inventory; one used in the shop comes back within a few seconds."}}) {
            CheckBox box=new CheckBox(activity);
            box.setText(m[1]);
            box.setTag(m[0]);
            box.setChecked(startup.contains(m[0]));
            boxes.add(box);list.addView(box);
            list.addView(note(m[2]));
        }
        Button selected = new Button(activity);
        selected.setText("Enter selected at title screen");
        selected.setOnClickListener(v -> {
            if(!save(boxes))return;
            List<Code> codes=new ArrayList<>();
            for(int i=0;i<CODES.length;i++)if(boxes.get(i).isChecked())codes.add(CODES[i]);
            if(codes.isEmpty())return;
            dialog[0].dismiss();type(codes.toArray(new Code[0]));
        });
        list.addView(selected);
        Button save = new Button(activity);
        save.setText("Save choices");
        save.setOnClickListener(v -> {if(save(boxes))Toast.makeText(activity,"Choices saved",Toast.LENGTH_SHORT).show();});
        list.addView(save);
        Button restart = new Button(activity);
        restart.setText("Save & Restart");
        restart.setOnClickListener(v -> new AlertDialog.Builder(activity)
                .setMessage("Save this code selection and restart? Unsaved race progress will be lost. No save file is deleted. Enabled USA hooks apply at startup. Enter bonus codes at Press Start after restarting.")
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Restart", (d,w) -> {if(save(boxes))restartGame();}).show());
        list.addView(restart);

        list.addView(heading("Good to know"));
        list.addView(note("Every cheat checks the USA game's code first and stays off on any other executable (the session log says which are active). Apply with Save & Restart."));

        style(list, restart);
        ScrollView scroll = new ScrollView(activity);
        scroll.addView(list);
        TextView title = MwTheme.text(activity, "CHEATS", 26, MwTheme.GOLD, MwTheme.DISPLAY);
        title.setPadding(pad16 + pad16 / 2, pad16, pad16, 0);
        dialog[0] = new AlertDialog.Builder(activity).setCustomTitle(title).setView(scroll)
                .setNegativeButton("Close", null).create();
        dialog[0].setOnShowListener(d -> {
            // The launcher's look (MwTheme): black, a gold edge, gold buttons.
            android.graphics.drawable.GradientDrawable frame = new android.graphics.drawable.GradientDrawable();
            frame.setColor(MwTheme.BLACK);
            frame.setStroke(MwTheme.dp(activity, 2), MwTheme.GOLD);
            frame.setCornerRadius(MwTheme.dp(activity, 8));
            dialog[0].getWindow().setBackgroundDrawable(frame);
            Button close = dialog[0].getButton(AlertDialog.BUTTON_NEGATIVE);
            if (close != null) { close.setTextColor(MwTheme.GOLD); close.setTypeface(MwTheme.DISPLAY); }
        });
        shown=dialog[0];
        dialog[0].setOnDismissListener(d -> {if(shown==dialog[0])shown=null;});
        dialog[0].show();
    }

    /** Black and gold, like the launcher: gold headings, muted notes, gold checkboxes, slanted buttons. */
    private void style(LinearLayout list, Button primary) {
        android.content.res.ColorStateList gold = android.content.res.ColorStateList.valueOf(MwTheme.GOLD);
        for (int i = 0; i < list.getChildCount(); i++) {
            android.view.View v = list.getChildAt(i);
            if (v instanceof CheckBox) {
                CheckBox box = (CheckBox) v;
                box.setTextColor(MwTheme.TEXT);
                box.setButtonTintList(gold);
                box.setTypeface(MwTheme.BODY);
                box.setTextSize(16);
            } else if (v instanceof Button) {
                Button b = (Button) v;
                Button styled = v == primary ? MwTheme.primary(activity, b.getText().toString()) : MwTheme.secondary(activity, b.getText().toString());
                b.setBackground(styled.getBackground());
                b.setTextColor(styled.getCurrentTextColor());
                b.setTypeface(MwTheme.DISPLAY);
                b.setAllCaps(!b.getText().toString().contains("\n"));  // a code's two lines keep their case
                b.setTextSize(v == primary ? 20 : 15);
                b.setStateListAnimator(null);
                b.setMinHeight(MwTheme.dp(activity, 52));
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
                lp.topMargin = MwTheme.dp(activity, 8);
                b.setLayoutParams(lp);
            } else if (v instanceof TextView) {
                TextView t = (TextView) v;
                if (t.getTag() == HEADING) { t.setTextColor(MwTheme.GOLD); t.setTypeface(MwTheme.DISPLAY); t.setAllCaps(true); }
                else { t.setTextColor(MwTheme.MUTED); t.setTypeface(MwTheme.BODY); }
            }
        }
    }

    private static final Object HEADING = new Object();

    private TextView heading(String text) {
        TextView t = new TextView(activity);
        t.setText(text);
        t.setTextSize(18);
        t.setPadding(0, (int) (12 * activity.getResources().getDisplayMetrics().density), 0, 0);
        t.setTag(HEADING);
        return t;
    }

    private TextView note(String text) {
        TextView t = new TextView(activity);
        t.setText(text);
        t.setAlpha(0.8f);
        return t;
    }

    // --- Typing the codes: one press and release per step, on the UI thread.

    private void type(Code[] codes) {
        if (typing || pad==null || activity.isFinishing()) return;
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
                handler.postDelayed(() -> {if(typing && pad.canInject())press(b);}, t);
                handler.postDelayed(() -> {if(typing)TouchGamepadView.inject(0, 0, 0);}, t + PRESS_MS);
                t += PRESS_MS + GAP_MS;
            }
        }
        handler.postDelayed(() -> {
            typing = false;
            if (pad != null) pad.reset();
            Toast.makeText(activity, "Done. A code that worked shows its unlock (or a sound) in the game.", Toast.LENGTH_LONG).show();
        }, t);
    }

    void focusLost() {
        handler.removeCallbacksAndMessages(null);typing=false;
        if(pad!=null)pad.reset();
    }

    void cancel() {
        handler.removeCallbacksAndMessages(null);
        typing=false;
        if(shown!=null){shown.dismiss();shown=null;}
        if(pad!=null)pad.reset();
    }

    private static void press(int button) {
        if (button == LT) TouchGamepadView.inject(0, 1, 0);
        else if (button == RT) TouchGamepadView.inject(0, 0, 1);
        else TouchGamepadView.inject(button, 0, 0);
    }

    // Saved code selection is separate from game saves and native startup patches.

    private File cheatsFile() {
        File data = new File(activity.getFilesDir(), "data");
        File old = new File(data, "nfsmw-recomp"), current = new File(data, "speedbreaker");
        File user = !current.isDirectory() && old.isDirectory() ? old : current;
        return new File(user, "title-code-selection.txt");
    }

    private List<String> readEnabled() {return read(cheatsFile());}
    private List<String> read(File file) {
        List<String> on = new ArrayList<>();
        try {
            for (String line : java.util.Arrays.asList(new String(new android.util.AtomicFile(file).readFully(),StandardCharsets.UTF_8).split("\\n")))
                if (line.trim().endsWith("=1")) on.add(line.trim().substring(0, line.trim().length() - 2));
        } catch (IOException ignored) { }
        return on;
    }

    private boolean save(List<CheckBox> boxes) {
        StringBuilder codes=new StringBuilder("# Saved title-screen code selection; entered manually.\n");
        StringBuilder startup=new StringBuilder("# USA-only optional startup hooks. Optional bonus content.\n");
        for(CheckBox b:boxes) {
            StringBuilder dest=b.getTag().toString().startsWith("code_")?codes:startup;
            dest.append(b.getTag()).append('=').append(b.isChecked()?1:0).append('\n');
        }
        return write(cheatsFile(),codes.toString()) && write(new File(cheatsFile().getParentFile(),"cheats.txt"),startup.toString());
    }
    private boolean write(File f,String text) {
        android.util.AtomicFile atomic=new android.util.AtomicFile(f);
        FileOutputStream out=null;
        try {
            File dir=f.getParentFile();
            if(!dir.isDirectory() && !dir.mkdirs())throw new IOException("Could not create settings folder");
            out=atomic.startWrite();
            out.write(text.getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
            atomic.finishWrite(out);out=null;
            // Verify the durable contents before allowing a process restart.
            if(!new String(atomic.readFully(),StandardCharsets.UTF_8).equals(text))throw new IOException("Settings verification failed");
            Log.i(TAG,"[cheats] title-code selection saved");
            return true;
        } catch(IOException e) {
            if(out!=null)atomic.failWrite(out);
            Toast.makeText(activity,"Could not save: "+e.getMessage(),Toast.LENGTH_LONG).show();
            return false;
        }
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
