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

/** Multitouch racing buttons and a local layout editor; no guest-memory hooks. */
final class TouchGamepadView extends View {
    private static native void nativeState(int buttons, float lx, float ly, float rx, float ry, float lt, float rt);
    private static native boolean nativeVisible();
    private static final int BACK=1<<4, START=1<<6;
    private static final int BUTTON=0, BRAKE=2, GAS=3, STEER_LEFT=4, STEER_RIGHT=5;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<Control> controls=new ArrayList<>();
    private final SparseArray<Finger> fingers=new SparseArray<>();
    private final SharedPreferences layout;
    private boolean running, editing, loggedDraw;
    private Boolean lastVisible;
    private Control selected;
    private int dragPointer=-1;
    private float dragOffsetX,dragOffsetY;
    private final Runnable visibilityCheck=new Runnable() {
        @Override public void run() {
            if(!running) return;
            boolean visible=nativeVisible();
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
        final String id,label; final int mask,kind; final RectF bounds; boolean down;
        Control(String key,String text,int buttons,int type,RectF rect) {id=key;label=text;mask=buttons;kind=type;bounds=rect;}
    }
    private static final class Finger {
        Control control; float x,y;
        Finger(Control c,float px,float py) {control=c;x=px;y=py;}
    }
    TouchGamepadView(Context context) {
        super(context);
        // Keep the proven overlay raster path independent of SDL's Vulkan surface.
        setLayerType(View.LAYER_TYPE_SOFTWARE,null);
        setFocusable(false);setContentDescription("Touch gamepad with editable racing buttons");
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        layout=context.getSharedPreferences("touch_layout_v1",Context.MODE_PRIVATE);
    }
    void resume() {running=true;removeCallbacks(visibilityCheck);post(visibilityCheck);}
    void pause() {running=false;removeCallbacks(visibilityCheck);reset();cancelEditor();}
    void reset() {fingers.clear();for(Control c:controls)c.down=false;nativeState(0,0,0,0,0,0,0);invalidate();}
    @Override protected void onDetachedFromWindow() {pause();super.onDetachedFromWindow();}
    private void button(String name,int mask,float x,float y,float radius) {
        controls.add(new Control(name,name,mask,BUTTON,new RectF(x-radius,y-radius,x+radius,y+radius)));
    }
    private void buildControls(boolean saved) {
        controls.clear();int w=getWidth(),h=getHeight();if(w<=0 || h<=0)return;
        float r=h*.052f;
        button("A",1,w*.87f,h*.79f,r);button("B",1<<1,w*.93f,h*.66f,r);
        button("X",1<<2,w*.81f,h*.66f,r);button("Y",1<<3,w*.87f,h*.53f,r);
        button("BACK",BACK,w*.41f,h*.12f,r);button("SET",BACK|START,w*.50f,h*.12f,r);
        button("START",START,w*.59f,h*.12f,r);
        button("LB",1<<9,w*.70f,h*.32f,r);button("RB",1<<10,w*.81f,h*.32f,r);
        button("UP",1<<11,w*.16f,h*.20f,r);button("DOWN",1<<12,w*.16f,h*.40f,r);
        button("LEFT",1<<13,w*.115f,h*.30f,r);button("RIGHT",1<<14,w*.205f,h*.30f,r);
        controls.add(new Control("steer-left","◀ LEFT",0,STEER_LEFT,new RectF(w*.055f,h*.70f,w*.145f,h*.90f)));
        controls.add(new Control("steer-right","RIGHT ▶",0,STEER_RIGHT,new RectF(w*.165f,h*.70f,w*.255f,h*.90f)));
        controls.add(new Control("brake","BRAKE / LT",0,BRAKE,new RectF(w*.55f,h*.72f,w*.65f,h*.89f)));
        controls.add(new Control("gas","GAS / RT",0,GAS,new RectF(w*.67f,h*.72f,w*.77f,h*.89f)));
        if(saved) for(Control c:controls) {
            float x=layout.getFloat(c.id+".x",c.bounds.centerX()/w),y=layout.getFloat(c.id+".y",c.bounds.centerY()/h);
            float cw=layout.getFloat(c.id+".w",c.bounds.width()/w),ch=layout.getFloat(c.id+".h",c.bounds.height()/h);
            if(Float.isFinite(x) && Float.isFinite(y) && Float.isFinite(cw) && Float.isFinite(ch) && cw>0 && ch>0) {
                c.bounds.set((x-cw/2)*w,(y-ch/2)*h,(x+cw/2)*w,(y+ch/2)*h);clamp(c);
            }
        }
        invalidate();
    }
    @Override protected void onSizeChanged(int w,int h,int oldW,int oldH) {
        reset();editing=false;selected=null;dragPointer=-1;buildControls(true);
    }
    private void clamp(Control c) {
        float w=getWidth(),h=getHeight();
        float cw=Math.max(h*.065f,Math.min(w*.30f,c.bounds.width()));
        float ch=Math.max(h*.065f,Math.min(h*.35f,c.bounds.height()));
        float x=Math.max(cw/2,Math.min(w-cw/2,c.bounds.centerX()));
        float y=Math.max(ch/2,Math.min(h-ch/2,c.bounds.centerY()));
        c.bounds.set(x-cw/2,y-ch/2,x+cw/2,y+ch/2);
    }
    private Control hit(float x,float y) {
        for(int i=controls.size()-1;i>=0;--i)if(controls.get(i).bounds.contains(x,y))return controls.get(i);
        return null;
    }
    private RectF editButton() {float w=getWidth(),h=getHeight();return new RectF(w*.87f,h*.035f,w*.97f,h*.12f);}
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
        SharedPreferences.Editor e=layout.edit();
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
        if(action==MotionEvent.ACTION_DOWN && editButton().contains(event.getX(),event.getY())){startEditor();return true;}
        if(action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
            Control c=hit(event.getX(index),event.getY(index));
            if(action==MotionEvent.ACTION_DOWN && c==null)return false;
            if(c!=null)fingers.put(id,new Finger(c,event.getX(index),event.getY(index)));
        }
        for(int i=0;i<event.getPointerCount();++i) {
            Finger f=fingers.get(event.getPointerId(i));
            if(f!=null) {
                f.x=event.getX(i);f.y=event.getY(i);
                // Slide between the driving buttons without lifting the thumb.
                if(f.control.kind==STEER_LEFT || f.control.kind==STEER_RIGHT) {
                    Control next=hit(f.x,f.y);
                    if(next!=null && (next.kind==STEER_LEFT || next.kind==STEER_RIGHT))f.control=next;
                }
            }
        }
        if(action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP)fingers.remove(id);
        publish();return true;
    }
    private void publish() {
        int buttons=0;float lt=0,rt=0;boolean left=false,right=false;
        for(Control c:controls)c.down=false;
        for(int i=0;i<fingers.size();++i) {
            Finger f=fingers.valueAt(i);Control c=f.control;
            if(c.bounds.contains(f.x,f.y)){c.down=true;buttons|=c.mask;}
        }
        for(Control c:controls)if(c.down) {
            if(c.kind==BRAKE)lt=1;if(c.kind==GAS)rt=1;
            left|=c.kind==STEER_LEFT;right|=c.kind==STEER_RIGHT;
        }
        // Opposite buttons cancel; menu D-pad remains independent of steering.
        nativeState(buttons,left==right?0:left?-1:1,0,0,0,lt,rt);invalidate();
    }
    private void drawButton(Canvas canvas,RectF b,String label,boolean down,boolean highlight) {
        float h=getHeight();paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(down?155:65,10,16,20));
        canvas.drawRoundRect(b,h*.025f,h*.025f,paint);
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(h*(highlight?.004f:.002f));
        paint.setColor(highlight?Color.rgb(255,200,60):Color.argb(down?235:125,255,255,255));
        canvas.drawRoundRect(b,h*.025f,h*.025f,paint);
        paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(230,255,255,255));paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(h*.025f);float tw=paint.measureText(label);
        if(tw>b.width()*.9f)paint.setTextSize(paint.getTextSize()*b.width()*.9f/tw);
        canvas.drawText(label,b.centerX(),b.centerY()-(paint.ascent()+paint.descent())/2,paint);
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if(!loggedDraw){android.util.Log.i("SpeedBreakerOne","[touch-ui] draw controls="+controls.size()+" hardware="+canvas.isHardwareAccelerated());loggedDraw=true;}
        if(editing)canvas.drawColor(Color.argb(130,0,0,0));
        for(Control c:controls)drawButton(canvas,c.bounds,c.label,c.down,editing && c==selected);
        if(editing) {
            String[] labels={"SAVE","RESET","CANCEL","SIZE −","SIZE +"};
            for(int i=0;i<labels.length;++i)drawButton(canvas,tool(i),labels[i],false,false);
            paint.setTextSize(getHeight()*.026f);paint.setColor(Color.WHITE);
            canvas.drawText("Drag a button. Select it, then use SIZE − / +.",getWidth()*.5f,getHeight()*.165f,paint);
        } else drawButton(canvas,editButton(),"EDIT",false,false);
    }
}
