// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

/**
 * The launcher's look: black and gold, slanted like a racing game's menus.
 * Everything is drawn in code (no game art ships in the app; the disc's own
 * icon comes from title_art in the game process).
 */
final class MwTheme {
    static final int BLACK = Color.rgb(8, 8, 10);
    static final int GOLD = Color.rgb(242, 183, 5);
    static final int GOLD_LIGHT = Color.rgb(255, 214, 92);
    static final int GOLD_DARK = Color.rgb(122, 90, 0);
    static final int TEXT = Color.rgb(236, 232, 220);
    static final int MUTED = Color.rgb(150, 143, 125);

    static final Typeface DISPLAY = Typeface.create("sans-serif-condensed", Typeface.BOLD_ITALIC);
    static final Typeface BODY = Typeface.create("sans-serif-condensed", Typeface.NORMAL);

    private MwTheme() { }

    static int dp(Context c, float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, c.getResources().getDisplayMetrics()));
    }

    static TextView text(Context c, String s, float sp, int color, Typeface face) {
        TextView t = new TextView(c);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        t.setTypeface(face);
        return t;
    }

    /** The big gold button (RACE). */
    static Button primary(Context c, String label) {
        Button b = base(c, label, 30);
        b.setTextColor(BLACK);
        b.setBackground(states(c, new Slant(c, GOLD, 0, GOLD_DARK), new Slant(c, GOLD_LIGHT, 0, GOLD_DARK),
                new Slant(c, GOLD, Color.WHITE, GOLD_DARK)));
        b.setMinHeight(dp(c, 84));
        return b;
    }

    /** A gold-outlined button. */
    static Button secondary(Context c, String label) {
        Button b = base(c, label, 17);
        b.setTextColor(GOLD);
        b.setBackground(states(c, new Slant(c, Color.argb(200, 18, 16, 10), GOLD, 0),
                new Slant(c, Color.argb(230, 60, 46, 6), GOLD_LIGHT, 0),
                new Slant(c, Color.argb(230, 40, 32, 8), Color.WHITE, 0)));
        b.setMinHeight(dp(c, 56));
        return b;
    }

    /** A garage row: label on the left, its value on the right. */
    static Button row(Context c) {
        Button b = secondary(c, "");
        b.setGravity(Gravity.CENTER_VERTICAL | Gravity.START);
        b.setTextSize(16);
        b.setAllCaps(false);
        int p = dp(c, 22);
        b.setPadding(p, 0, p, 0);
        return b;
    }

    private static Button base(Context c, String label, float sp) {
        Button b = new Button(c);
        b.setText(label);
        b.setAllCaps(true);
        b.setTextSize(sp);
        b.setTypeface(DISPLAY);
        b.setStateListAnimator(null);
        b.setFocusable(true);
        b.setLetterSpacing(0.04f);
        return b;
    }

    private static StateListDrawable states(Context c, Drawable normal, Drawable pressed, Drawable focused) {
        StateListDrawable s = new StateListDrawable();
        s.addState(new int[] {android.R.attr.state_pressed}, pressed);
        s.addState(new int[] {android.R.attr.state_focused}, focused);
        s.addState(new int[] {-android.R.attr.state_enabled}, new Slant(c, Color.argb(120, 40, 40, 40), Color.argb(90, 120, 120, 120), 0));
        s.addState(new int[] {}, normal);
        return s;
    }

    /** A parallelogram, slanted like a speed stripe, with an optional outline and shadow edge. */
    static final class Slant extends Drawable {
        private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG), line = new Paint(Paint.ANTI_ALIAS_FLAG),
                edge = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final float slant, stroke;
        private final Path path = new Path();

        Slant(Context c, int fillColor, int lineColor, int edgeColor) {
            slant = dp(c, 14);
            stroke = dp(c, 2);
            fill.setColor(fillColor);
            line.setStyle(Paint.Style.STROKE);
            line.setStrokeWidth(stroke);
            line.setColor(lineColor);
            edge.setColor(edgeColor);
        }

        @Override public void draw(Canvas canvas) {
            Rect r = getBounds();
            float inset = stroke;
            path.reset();
            path.moveTo(r.left + slant + inset, r.top + inset);
            path.lineTo(r.right - inset, r.top + inset);
            path.lineTo(r.right - slant - inset, r.bottom - inset);
            path.lineTo(r.left + inset, r.bottom - inset);
            path.close();
            if (Color.alpha(edge.getColor()) != 0) {
                canvas.save();
                canvas.translate(stroke * 2, stroke * 2);
                canvas.drawPath(path, edge);
                canvas.restore();
            }
            canvas.drawPath(path, fill);
            if (Color.alpha(line.getColor()) != 0) canvas.drawPath(path, line);
        }

        @Override public void setAlpha(int alpha) { fill.setAlpha(alpha); }
        @Override public void setColorFilter(ColorFilter f) { fill.setColorFilter(f); }
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    /** The backdrop: black, a warm glow behind the art, faint speed stripes, a gold rule along the bottom. */
    static final class Backdrop extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private android.graphics.Bitmap picture;

        Backdrop(Context c) { super(c); }

        /** The player's own background picture (Garage), or null for plain black. */
        void setPicture(android.graphics.Bitmap bitmap) { picture = bitmap; invalidate(); }

        @Override protected void onDraw(Canvas canvas) {
            int w = getWidth(), h = getHeight();
            canvas.drawColor(BLACK);
            if (picture != null) {
                // Centre-cropped and faded, darker toward the buttons on the right so they stay readable.
                float scale = Math.max(w / (float) picture.getWidth(), h / (float) picture.getHeight());
                float pw = picture.getWidth() * scale, ph = picture.getHeight() * scale;
                android.graphics.RectF dst = new android.graphics.RectF((w - pw) / 2, (h - ph) / 2, (w + pw) / 2, (h + ph) / 2);
                paint.setAlpha(110);
                canvas.drawBitmap(picture, null, dst, paint);
                paint.setAlpha(255);
                paint.setShader(new LinearGradient(0, 0, w, 0, new int[] {Color.argb(60, 8, 8, 10), Color.argb(150, 8, 8, 10), Color.argb(215, 8, 8, 10)},
                        new float[] {0f, 0.45f, 1f}, Shader.TileMode.CLAMP));
                canvas.drawRect(0, 0, w, h, paint);
                paint.setShader(new LinearGradient(0, 0, 0, h, new int[] {Color.argb(120, 8, 8, 10), Color.TRANSPARENT, Color.argb(160, 8, 8, 10)},
                        new float[] {0f, 0.35f, 1f}, Shader.TileMode.CLAMP));
                canvas.drawRect(0, 0, w, h, paint);
                paint.setShader(null);
            }
            paint.setShader(new RadialGradient(w * 0.24f, h * 0.45f, Math.max(w, h) * 0.55f,
                    Color.argb(120, 120, 86, 0), Color.TRANSPARENT, Shader.TileMode.CLAMP));
            canvas.drawRect(0, 0, w, h, paint);
            paint.setShader(null);
            paint.setStrokeWidth(dp(getContext(), 2));
            paint.setColor(Color.argb(14, 242, 183, 5));
            for (int x = -h; x < w; x += dp(getContext(), 26)) canvas.drawLine(x, h, x + h * 0.6f, 0, paint);
            paint.setShader(new LinearGradient(0, 0, w, 0, new int[] {Color.TRANSPARENT, GOLD, Color.TRANSPARENT}, null, Shader.TileMode.CLAMP));
            float y = h - dp(getContext(), 6);
            canvas.drawRect(0, y, w, y + dp(getContext(), 2), paint);
            paint.setShader(null);
        }
    }
}
