// SpeedBreaker One. GPL-3.0-or-later.
package com.onenonlygit.speedbreakerone;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;
import java.util.ArrayList;

/**
 * Multitouch controls in two layouts, each with its own editor and saved
 * positions; no guest-memory hooks.
 *
 *   RACING  what driving needs, labelled by what it does: GAS (RT), BRAKE
 *           (LT), ◀ ▶ steering (left stick x), NITRO (B), HANDBRAKE (A),
 *           LOOK BACK (Y), SPEEDBREAKER (X), PAUSE (START), BACK, and ▲ ▼
 *           for the menus.
 *   XBOX    the Xbox 360 controller as it is: both sticks, D-pad, A B X Y,
 *           LB RB LT RT, BACK and START.
 *
 * The bar at the top switches between them at any time, opens the editor for
 * the one shown (drag, SIZE − / +, SAVE, RESET, CANCEL), the CHEATS menu and
 * SpeedBreaker's own SETTINGS (frame rate, quality: Back + Start).
 * Layouts are saved as fractions of the screen, per layout
 * (touch_layout_racing, touch_layout_xbox); the one shown, in touch_layouts.
 *
 * In preview (the launcher's controls editor, without the game) nothing is
 * sent to native code.
 */
final class TouchGamepadView extends View {
    private static native void nativeState(int buttons, float lx, float ly, float rx, float ry, float lt, float rt);
    private static native boolean nativeVisible();

    // Haptics (launcher Garage, prefs "haptics"): a tick when a finger lands on a
    // button, and the game's own rumble on the phone's motor (rumble(), called by
    // the native touch pad's SDL rumble, runtime/platform/android/touch_gamepad.cpp).
    static final String HAPTICS = "haptics";
    private static android.os.Vibrator vibrator;
    private static volatile boolean rumbleOn = true;
    private static boolean hasAmplitude;
    private boolean tapOn = true;

    /** The game's rumble, 0..255 (0 stops). Any thread; the native side sends only changes. */
    static void rumble(int amplitude) {
        android.os.Vibrator v = vibrator;
        if (v == null) return;
        if (amplitude <= 0 || !rumbleOn) { v.cancel(); return; }
        int a = hasAmplitude ? Math.max(1, Math.min(255, amplitude)) : android.os.VibrationEffect.DEFAULT_AMPLITUDE;
        // Held until the next change: a repeating step, not a one-shot that runs out.
        v.vibrate(android.os.VibrationEffect.createWaveform(new long[] {1000}, new int[] {a}, 0),
                new android.os.VibrationAttributes.Builder().setUsage(android.os.VibrationAttributes.USAGE_MEDIA).build());
    }
    /** Drives the virtual pad directly: CheatsMenu types the title-screen codes with it. */
    static void inject(int buttons, float lt, float rt) { nativeState(buttons,0,0,0,0,lt,rt); }

    // SDL gamepad buttons.
    private static final int A=1, B=1<<1, X=1<<2, Y=1<<3, BACK=1<<4, START=1<<6,
            LB=1<<9, RB=1<<10, UP=1<<11, DOWN=1<<12, LEFT=1<<13, RIGHT=1<<14;
    private static final int BUTTON=0, BRAKE=2, GAS=3, STEER_LEFT=4, STEER_RIGHT=5, STICK_L=6, STICK_R=7;
    static final String RACING="racing", XBOX="xbox";

    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<Control> controls=new ArrayList<>();
    private final SparseArray<Finger> fingers=new SparseArray<>();
    private final SharedPreferences settings;
    private final boolean preview;
    private String layoutName;
    private Runnable onCheats;
    private boolean running, editing, loggedDraw;
    private Boolean lastVisible;
    private Control selected;
    private int dragPointer=-1;
    private float dragOffsetX,dragOffsetY;
    private final Runnable visibilityCheck=new Runnable() {
        @Override public void run() {
            if(!running) return;
            boolean visible=preview || nativeVisible();
            if(lastVisible==null || visible!=lastVisible) {
                android.util.Log.i("SpeedBreakerOne", "[touch-ui] native visible="+visible+" size="+getWidth()+"x"+getHeight());
                lastVisible=visible;
            }
            if(!visible && (fingers.size()>0 || editing)) { reset();cancelEditor(); }
            setVisibility(visible?VISIBLE:INVISIBLE);
            postDelayed(this,100);
        }
    };
    private static final class Control {
        final String id,label,sub; final int mask,kind; final boolean round; final RectF bounds; boolean down;
        float knobX,knobY;  // sticks: -1..1
        Control(String key,String text,String small,int buttons,int type,boolean circle,RectF rect) {
            id=key;label=text;sub=small;mask=buttons;kind=type;round=circle;bounds=rect;
        }
    }
    private static final class Finger {
        Control control; float x,y;
        Finger(Control c,float px,float py) {control=c;x=px;y=py;}
    }

    TouchGamepadView(Context context) { this(context,false); }
    TouchGamepadView(Context context,boolean previewOnly) {
        super(context);
        preview=previewOnly;
        // Keep the proven overlay raster path independent of SDL's Vulkan surface.
        setLayerType(View.LAYER_TYPE_SOFTWARE,null);
        setFocusable(false);setContentDescription("Touch gamepad: racing or Xbox layout, editable");
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        settings=context.getSharedPreferences("touch_layouts",Context.MODE_PRIVATE);
        // The old SAVED layout (the first vivo build's, prefs touch_layout_v1) is
        // gone: the owner keeps RACING and XBOX. Its positions are deleted and a
        // player who had it active lands on RACING.
        android.content.SharedPreferences old=context.getSharedPreferences("touch_layout_v1",Context.MODE_PRIVATE);
        if(!old.getAll().isEmpty())old.edit().clear().apply();
        layoutName=XBOX.equals(settings.getString("active",RACING))?XBOX:RACING;
        android.content.SharedPreferences haptics=context.getSharedPreferences(HAPTICS,Context.MODE_PRIVATE);
        tapOn=haptics.getBoolean("tap",true);
        if(!previewOnly) {
            rumbleOn=haptics.getBoolean("rumble",true);
            android.os.VibratorManager manager=context.getSystemService(android.os.VibratorManager.class);
            vibrator=manager!=null?manager.getDefaultVibrator():null;
            hasAmplitude=vibrator!=null && vibrator.hasAmplitudeControl();
        }
    }
    /** The top bar's CHEATS button; without one it isn't drawn. */
    void setOnCheats(Runnable action) {onCheats=action;invalidate();}
    boolean canInject() {return !preview && running && nativeVisible();}
    void resume() {running=true;removeCallbacks(visibilityCheck);post(visibilityCheck);}
    void pause() {
        if(!preview)rumble(0);  // never buzz on behind another app
        running=false;removeCallbacks(visibilityCheck);reset();cancelEditor();
    }
    void reset() {fingers.clear();for(Control c:controls){c.down=false;c.knobX=c.knobY=0;}send(0,0,0,0,0,0,0);invalidate();}
    private void send(int buttons,float lx,float ly,float rx,float ry,float lt,float rt) {
        if(!preview) nativeState(buttons,lx,ly,rx,ry,lt,rt);
    }
    @Override protected void onDetachedFromWindow() {pause();super.onDetachedFromWindow();}
    private SharedPreferences layout() {
        return getContext().getSharedPreferences("touch_layout_"+layoutName,Context.MODE_PRIVATE);
    }

    // --- The two layouts. Positions are fractions of the screen; buttons are
    // sized from the height so they stay round on any aspect ratio.

    private void round(String id,String label,String sub,int mask,float cx,float cy,float r) {
        float w=getWidth(),h=getHeight(),rad=h*r;
        controls.add(new Control(id,label,sub,mask,BUTTON,true,new RectF(cx*w-rad,cy*h-rad,cx*w+rad,cy*h+rad)));
    }
    private void box(String id,String label,String sub,int mask,int kind,float l,float t,float r,float b) {
        float w=getWidth(),h=getHeight();
        controls.add(new Control(id,label,sub,mask,kind,false,new RectF(l*w,t*h,r*w,b*h)));
    }
    private void stick(String id,String label,int kind,float cx,float cy,float r) {
        float w=getWidth(),h=getHeight(),rad=h*r;
        controls.add(new Control(id,label,null,0,kind,true,new RectF(cx*w-rad,cy*h-rad,cx*w+rad,cy*h+rad)));
    }
    private String layoutLabel() {return XBOX.equals(layoutName)?"XBOX":"RACING";}
    private void racingDefaults() {
        box("steer-left","◀",null,0,STEER_LEFT,.025f,.60f,.135f,.93f);
        box("steer-right","▶",null,0,STEER_RIGHT,.150f,.60f,.260f,.93f);
        round("menu-up","▲","menus",UP,.055f,.36f,.055f);
        round("menu-down","▼","menus",DOWN,.055f,.50f,.055f);
        box("gas","GAS","RT",0,GAS,.875f,.52f,.980f,.93f);
        box("brake","BRAKE","LT",0,BRAKE,.755f,.66f,.860f,.93f);
        round("nitro","NITRO","B",B,.930f,.375f,.075f);
        round("handbrake","HANDBRAKE","A",A,.805f,.515f,.070f);
        round("speedbreaker","SLOW TIME","X",X,.680f,.62f,.065f);
        round("lookback","LOOK BACK","Y",Y,.805f,.300f,.060f);
        round("pause","PAUSE","START",START,.055f,.17f,.050f);
        round("back","BACK",null,BACK,.140f,.17f,.050f);
    }
    private void xboxDefaults() {
        stick("lstick","L",STICK_L,.13f,.66f,.17f);
        round("up","▲",null,UP,.31f,.60f,.055f);
        round("down","▼",null,DOWN,.31f,.86f,.055f);
        round("left","◀",null,LEFT,.255f,.73f,.055f);
        round("right","▶",null,RIGHT,.365f,.73f,.055f);
        stick("rstick","R",STICK_R,.62f,.78f,.13f);
        round("y","Y",null,Y,.875f,.44f,.060f);
        round("x","X",null,X,.810f,.57f,.060f);
        round("b","B",null,B,.940f,.57f,.060f);
        round("a","A",null,A,.875f,.70f,.060f);
        box("lt","LT",null,0,BRAKE,.025f,.14f,.145f,.25f);
        box("lb","LB",null,LB,BUTTON,.025f,.27f,.145f,.36f);
        box("rt","RT",null,0,GAS,.855f,.14f,.975f,.25f);
        box("rb","RB",null,RB,BUTTON,.855f,.27f,.975f,.36f);
        round("back","BACK",null,BACK,.43f,.22f,.045f);
        round("start","START",null,START,.57f,.22f,.045f);
    }
    private void buildControls(boolean saved) {
        controls.clear();int w=getWidth(),h=getHeight();if(w<=0 || h<=0)return;
        if(XBOX.equals(layoutName))xboxDefaults();else racingDefaults();
        if(saved) {
            SharedPreferences layout=layout();
            for(Control c:controls) {
                float x=layout.getFloat(c.id+".x",c.bounds.centerX()/w),y=layout.getFloat(c.id+".y",c.bounds.centerY()/h);
                float cw=layout.getFloat(c.id+".w",c.bounds.width()/w),ch=layout.getFloat(c.id+".h",c.bounds.height()/h);
                if(Float.isFinite(x) && Float.isFinite(y) && Float.isFinite(cw) && Float.isFinite(ch) && cw>0 && ch>0) {
                    c.bounds.set((x-cw/2)*w,(y-ch/2)*h,(x+cw/2)*w,(y+ch/2)*h);clamp(c);
                }
            }
        }
        invalidate();
    }
    private void switchLayout() {
        reset();
        String next=RACING.equals(layoutName)?XBOX:RACING;
        if(!settings.edit().putString("active",next).commit())return;
        layoutName=next;
        android.util.Log.i("SpeedBreakerOne","[touch-ui] layout "+layoutName);
        buildControls(true);
    }
    @Override protected void onSizeChanged(int w,int h,int oldW,int oldH) {
        reset();editing=false;selected=null;dragPointer=-1;buildControls(true);
    }
    private void clamp(Control c) {
        float w=getWidth(),h=getHeight();
        float cw=Math.max(h*.065f,Math.min(w*.30f,c.bounds.width()));
        float ch=Math.max(h*.065f,Math.min(h*.50f,c.bounds.height()));
        float x=Math.max(cw/2,Math.min(w-cw/2,c.bounds.centerX()));
        float y=Math.max(ch/2,Math.min(h-ch/2,c.bounds.centerY()));
        c.bounds.set(x-cw/2,y-ch/2,x+cw/2,y+ch/2);
    }
    private Control hit(float x,float y) {
        for(int i=controls.size()-1;i>=0;--i)if(controls.get(i).bounds.contains(x,y))return controls.get(i);
        return null;
    }

    // --- The top bar: layout switch (centre), EDIT and CHEATS (right).
    private RectF layoutButton() {float w=getWidth(),h=getHeight();return new RectF(w*.38f,h*.025f,w*.62f,h*.105f);}
    private RectF editButton() {float w=getWidth(),h=getHeight();return new RectF(w*.755f,h*.025f,w*.855f,h*.105f);}
    private RectF cheatsButton() {float w=getWidth(),h=getHeight();return new RectF(w*.87f,h*.025f,w*.975f,h*.105f);}
    private RectF settingsButton() {float w=getWidth(),h=getHeight();return new RectF(w*.025f,h*.025f,w*.145f,h*.105f);}
    private RectF tool(int i) {
        float w=getWidth(),h=getHeight(),x=w*(.19f+i*.125f);
        return new RectF(x,h*.025f,x+w*.115f,h*.115f);
    }

    private void startEditor() {reset();editing=true;selected=null;dragPointer=-1;invalidate();}
    private void cancelEditor() {
        if(!editing)return;
        editing=false;selected=null;dragPointer=-1;buildControls(true);
    }
    private void saveEditor() {
        SharedPreferences.Editor e=layout().edit();
        for(Control c:controls) {
            e.putFloat(c.id+".x",c.bounds.centerX()/getWidth()).putFloat(c.id+".y",c.bounds.centerY()/getHeight());
            e.putFloat(c.id+".w",c.bounds.width()/getWidth()).putFloat(c.id+".h",c.bounds.height()/getHeight());
        }
        // SAVE must be durable before SDL's Activity can terminate its process.
        // This is a small, explicit user action, not a per-frame write.
        if(!e.commit()) {
            android.widget.Toast.makeText(getContext(),"Could not save the layout. Please try again.",android.widget.Toast.LENGTH_SHORT).show();
            return;
        }
        android.widget.Toast.makeText(getContext(),layoutLabel()+" layout saved",android.widget.Toast.LENGTH_SHORT).show();
        editing=false;selected=null;dragPointer=-1;reset();
    }
    private void resizeSelected(float factor) {
        if(selected==null)return;
        RectF b=selected.bounds;float x=b.centerX(),y=b.centerY(),w=b.width()*factor,h=b.height()*factor;
        b.set(x-w/2,y-h/2,x+w/2,y+h/2);clamp(selected);invalidate();
    }
    private boolean editTouch(MotionEvent e) {
        int a=e.getActionMasked(),i=e.getActionIndex();
        if(a==MotionEvent.ACTION_CANCEL) {dragPointer=-1;return true;}
        if(a==MotionEvent.ACTION_DOWN) {
            float x=e.getX(),y=e.getY();
            for(int k=0;k<5;++k)if(tool(k).contains(x,y)) {
                if(k==0)saveEditor();else if(k==1){selected=null;buildControls(false);}
                else if(k==2)cancelEditor();else resizeSelected(k==3?.9f:1.1f);
                return true;
            }
            selected=hit(x,y);dragPointer=selected==null?-1:e.getPointerId(0);
            if(selected!=null){dragOffsetX=x-selected.bounds.centerX();dragOffsetY=y-selected.bounds.centerY();}
            invalidate();
        } else if(a==MotionEvent.ACTION_MOVE && dragPointer>=0 && selected!=null) {
            int at=e.findPointerIndex(dragPointer);
            if(at>=0) {
                selected.bounds.offsetTo(e.getX(at)-dragOffsetX-selected.bounds.width()/2,e.getY(at)-dragOffsetY-selected.bounds.height()/2);
                clamp(selected);invalidate();
            }
        } else if((a==MotionEvent.ACTION_UP || a==MotionEvent.ACTION_POINTER_UP) && e.getPointerId(i)==dragPointer)dragPointer=-1;
        return true;
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if(!running || getVisibility()!=VISIBLE)return false;
        if(editing)return editTouch(event);
        int action=event.getActionMasked(),index=event.getActionIndex(),id=event.getPointerId(index);
        if(action==MotionEvent.ACTION_CANCEL){reset();return true;}
        if(action==MotionEvent.ACTION_DOWN) {
            float x=event.getX(),y=event.getY();
            if(layoutButton().contains(x,y)){switchLayout();return true;}
            if(editButton().contains(x,y)){startEditor();return true;}
            if(onCheats!=null && cheatsButton().contains(x,y)){reset();onCheats.run();return true;}
            if(!preview && settingsButton().contains(x,y)) {
                // SpeedBreaker's own settings (frame rate, quality...): its Back + Start
                // chord (ui/ui.cpp), pressed briefly, then released from the fingers' state.
                reset();send(BACK|START,0,0,0,0,0,0);postDelayed(this::publish,150);return true;
            }
        }
        if(action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
            Control c=hit(event.getX(index),event.getY(index));
            if(action==MotionEvent.ACTION_DOWN && c==null)return false;
            if(c!=null) {
                fingers.put(id,new Finger(c,event.getX(index),event.getY(index)));
                tick();
            }
        }
        for(int i=0;i<event.getPointerCount();++i) {
            Finger f=fingers.get(event.getPointerId(i));
            if(f!=null) {
                f.x=event.getX(i);f.y=event.getY(i);
                // Slide between the driving buttons without lifting the thumb.
                if(f.control.kind==STEER_LEFT || f.control.kind==STEER_RIGHT) {
                    Control next=hit(f.x,f.y);
                    if(next!=null && (next.kind==STEER_LEFT || next.kind==STEER_RIGHT) && next!=f.control){f.control=next;tick();}
                }
            }
        }
        if(action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP)fingers.remove(id);
        publish();return true;
    }
    /** A light tick for a press, as a key on the system keyboard (and the system's own haptics setting). */
    private void tick() { if(tapOn)performHapticFeedback(android.view.HapticFeedbackConstants.KEYBOARD_TAP); }

    private void publish() {
        int buttons=0;float lt=0,rt=0,lx=0,ly=0,rx=0,ry=0;boolean left=false,right=false;
        for(Control c:controls){c.down=false;c.knobX=c.knobY=0;}
        for(int i=0;i<fingers.size();++i) {
            Finger f=fingers.valueAt(i);Control c=f.control;
            if(c.kind==STICK_L || c.kind==STICK_R) {
                // A stick keeps its finger anywhere until it lifts, like a thumb that slips off.
                float r=c.bounds.width()/2,dx=(f.x-c.bounds.centerX())/r,dy=(f.y-c.bounds.centerY())/r;
                float len=(float)Math.hypot(dx,dy);if(len>1){dx/=len;dy/=len;}
                c.down=true;c.knobX=dx;c.knobY=dy;
            } else if(c.bounds.contains(f.x,f.y)){c.down=true;buttons|=c.mask;}
        }
        for(Control c:controls)if(c.down) {
            if(c.kind==BRAKE)lt=1;if(c.kind==GAS)rt=1;
            left|=c.kind==STEER_LEFT;right|=c.kind==STEER_RIGHT;
            if(c.kind==STICK_L){lx=c.knobX;ly=c.knobY;}
            if(c.kind==STICK_R){rx=c.knobX;ry=c.knobY;}
        }
        // Opposite steering buttons cancel; they drive the left stick's x.
        if(left!=right)lx=left?-1:1;
        send(buttons,lx,ly,rx,ry,lt,rt);invalidate();
    }

    // --- Drawing.
    private void drawButton(Canvas canvas,Control c,boolean highlight) {
        drawShape(canvas,c.bounds,c.round,c.down,highlight);
        float h=getHeight();
        if(c.kind==STICK_L || c.kind==STICK_R) {
            float r=c.bounds.width()/2,kr=r*.42f;
            paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(c.down?200:120,200,205,215));
            canvas.drawCircle(c.bounds.centerX()+c.knobX*(r-kr),c.bounds.centerY()+c.knobY*(r-kr),kr,paint);
        }
        label(canvas,c.bounds,c.label,c.sub,h);
    }
    private void drawShape(Canvas canvas,RectF b,boolean round,boolean down,boolean highlight) {
        float h=getHeight();float corner=round?b.width()/2:h*.025f;
        paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(down?155:65,10,16,20));
        canvas.drawRoundRect(b,corner,corner,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(h*(highlight?.004f:.002f));
        paint.setColor(highlight?Color.rgb(255,200,60):Color.argb(down?235:125,255,255,255));
        canvas.drawRoundRect(b,corner,corner,paint);
    }
    private void label(Canvas canvas,RectF b,String text,String sub,float h) {
        paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(230,255,255,255));paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(h*.025f);float tw=paint.measureText(text);
        if(tw>b.width()*.86f)paint.setTextSize(paint.getTextSize()*b.width()*.86f/tw);
        float y=b.centerY()-(paint.ascent()+paint.descent())/2;
        if(sub!=null)y-=h*.011f;
        canvas.drawText(text,b.centerX(),y,paint);
        if(sub!=null) {
            paint.setTextSize(h*.018f);paint.setColor(Color.argb(170,255,255,255));
            canvas.drawText(sub,b.centerX(),y+h*.026f,paint);
        }
    }
    private void barButton(Canvas canvas,RectF b,String text) {
        drawShape(canvas,b,false,false,false);label(canvas,b,text,null,getHeight());
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if(!loggedDraw){android.util.Log.i("SpeedBreakerOne","[touch-ui] draw controls="+controls.size()+" layout="+layoutName+" hardware="+canvas.isHardwareAccelerated());loggedDraw=true;}
        if(editing)canvas.drawColor(Color.argb(130,0,0,0));
        for(Control c:controls)drawButton(canvas,c,editing && c==selected);
        if(editing) {
            String[] labels={"SAVE","RESET","CANCEL","SIZE −","SIZE +"};
            for(int i=0;i<labels.length;++i)barButton(canvas,tool(i),labels[i]);
            paint.setTextSize(getHeight()*.026f);paint.setColor(Color.WHITE);paint.setTextAlign(Paint.Align.CENTER);
            canvas.drawText("Editing the "+layoutLabel()+" layout: drag a button; select it, then SIZE − / +.",
                    getWidth()*.5f,getHeight()*.165f,paint);
        } else {
            barButton(canvas,layoutButton(),layoutLabel()+" LAYOUT ⇄");
            barButton(canvas,editButton(),"EDIT");
            if(onCheats!=null)barButton(canvas,cheatsButton(),"CHEATS");
            if(!preview)barButton(canvas,settingsButton(),"SETTINGS");
        }
    }
}
