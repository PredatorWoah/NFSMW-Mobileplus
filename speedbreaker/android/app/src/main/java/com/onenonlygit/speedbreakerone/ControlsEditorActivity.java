// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

/**
 * The touch controls outside the game, from the launcher: switch between the
 * RACING and XBOX layouts, edit and save each one, without starting the game
 * (and in an APK without it). Landscape and full screen like the game, so the
 * saved fractions land in the same places. Nothing is sent to native code.
 */
public final class ControlsEditorActivity extends Activity {
    private TouchGamepadView controls;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(params);
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.rgb(18, 22, 28));
        TextView hint = new TextView(this);
        hint.setText("Touch controls preview\n\nSwitch layouts at the top, tap EDIT to move and resize buttons, then SAVE.\n"
                + "Each layout is saved on its own. Press Back to leave.");
        hint.setTextColor(Color.argb(150, 255, 255, 255));
        hint.setTextSize(16);
        hint.setGravity(Gravity.CENTER);
        root.addView(hint, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        controls = new TouchGamepadView(this, true);
        root.addView(controls, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(root);
    }

    @Override protected void onResume() { super.onResume(); hideSystemBars(); controls.resume(); }
    @Override protected void onPause() { controls.pause(); super.onPause(); }

    private void hideSystemBars() {
        getWindow().setDecorFitsSystemWindows(false);
        WindowInsetsController c = getWindow().getInsetsController();
        if (c != null) {
            c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            c.hide(WindowInsets.Type.systemBars());
        }
    }
}
