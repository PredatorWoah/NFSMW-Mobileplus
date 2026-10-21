// SpeedBreaker One. GPL-3.0-or-later.
// UI callbacks publish snapshots only; SDL devices belong to the SDL thread.
#include "touch_gamepad.h"
#include <jni.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace {
struct State { uint32_t buttons=0; float axes[6]{}; };
std::mutex mutex;
State state;
std::atomic<bool> ready{false}, blocked{false};
SDL_Joystick* joystick=nullptr;
SDL_JoystickID id=0;
float Limit(float v,float lo,float hi) { return std::isfinite(v)?std::clamp(v,lo,hi):0; }

// The game's rumble on the phone's motor: TouchGamepadView.rumble(int), its
// class and method cached from a UI-thread call (the app's class loader; a
// native thread's FindClass wouldn't see it).
jclass viewClass=nullptr;
jmethodID rumbleMethod=nullptr;
std::atomic<int> lastAmplitude{-1};
void CacheView(JNIEnv* env,jclass type) {
    if(viewClass) return;
    viewClass=static_cast<jclass>(env->NewGlobalRef(type));
    rumbleMethod=env->GetStaticMethodID(viewClass,"rumble","(I)V");
    if(!rumbleMethod) env->ExceptionClear();
}
// SDL_RumbleJoystick on the touch pad (hid/input.cpp sends the game's
// request, which changes every frame while driving): the stronger motor
// sets the strength, in 16 steps, and only a change goes to Java.
bool SDLCALL Rumble(void*,Uint16 low,Uint16 high) {
    const int amplitude=std::max(int(low)*3/4,int(high))*255/65535;
    const int stepped=amplitude==0?0:std::max(16,amplitude/16*16);
    if(lastAmplitude.exchange(stepped)==stepped || !viewClass || !rumbleMethod) return true;
    auto* env=static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if(!env) return false;
    env->CallStaticVoidMethod(viewClass,rumbleMethod,jint(stepped));
    if(env->ExceptionCheck()) env->ExceptionClear();
    return true;
}
}
extern "C" JNIEXPORT void JNICALL
Java_com_onenonlygit_speedbreakerone_TouchGamepadView_nativeState(JNIEnv* env,jclass type,jint buttons,
        jfloat lx,jfloat ly,jfloat rx,jfloat ry,jfloat lt,jfloat rt) {
    CacheView(env,type);
    std::lock_guard lock(mutex);
    state={uint32_t(buttons),{Limit(lx,-1,1),Limit(ly,-1,1),Limit(rx,-1,1),Limit(ry,-1,1),Limit(lt,0,1),Limit(rt,0,1)}};
}
extern "C" JNIEXPORT jboolean JNICALL
Java_com_onenonlygit_speedbreakerone_TouchGamepadView_nativeVisible(JNIEnv*,jclass) {
    return ready.load(std::memory_order_acquire) && !blocked.load(std::memory_order_relaxed);
}
namespace platform::android::touch {
void Clear() { std::lock_guard lock(mutex);state={}; }
bool IsPad(SDL_JoystickID other) { return id && other==id; }
void Update() {
    if(!joystick) return;
    State sample;
    { std::lock_guard lock(mutex);sample=state; }
    if(blocked.load(std::memory_order_relaxed)) sample={};
    for(int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;++b)
        SDL_SetJoystickVirtualButton(joystick,b,(sample.buttons&(1u<<b))!=0);
    for(int a=0;a<SDL_GAMEPAD_AXIS_COUNT;++a) {
        // SDL virtual trigger axes span -32768..32767; gamepad APIs remap
        // them to 0..32767. Zero here would leave the gas half pressed.
        const float value=a<4 ? sample.axes[a]*32767.0f : sample.axes[a]*65535.0f-32768.0f;
        SDL_SetJoystickVirtualAxis(joystick,a,Sint16(value));
    }
    SDL_UpdateJoysticks();
    static uint32_t lastButtons=0;
    static uint64_t lastLog=0;
    const uint64_t now=SDL_GetTicks();
    bool active=sample.buttons!=0;
    for(float axis:sample.axes) active|=axis!=0;
    if(sample.buttons!=lastButtons || (active && now-lastLog>=1000)) {
        fprintf(stderr,"[touch] SDL snapshot buttons=%08x stick=(%.2f,%.2f) LT=%.2f RT=%.2f\n",
            sample.buttons,sample.axes[0],sample.axes[1],sample.axes[4],sample.axes[5]);
        lastButtons=sample.buttons;lastLog=now;
    }
}
void Initialize() {
    if(std::getenv("NFSMW_VIRTUAL_PAD")) return; // preserve scripted tests
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.name="SpeedBreaker touch gamepad";
    desc.Rumble=Rumble;  // the phone's vibration motor (TouchGamepadView.rumble)
    id=SDL_AttachVirtualJoystick(&desc);
    joystick=id?SDL_OpenJoystick(id):nullptr;
    if(!joystick) { fprintf(stderr,"[touch] attach failed: %s\n",SDL_GetError());return; }
    Clear();Update();
    ready.store(true,std::memory_order_release);
    fprintf(stderr,"[touch] virtual gamepad attached; input follows existing SDL/XInput path\n");
}
void SetBlocked(bool value) {
    if(blocked.exchange(value,std::memory_order_relaxed)!=value) {
        fprintf(stderr,"[touch] native UI blocking=%d\n",int(value));
        if(value) Clear();
    }
}
void Shutdown() {
    ready.store(false,std::memory_order_release);
    Clear();Update();
    if(joystick) SDL_CloseJoystick(joystick);
    joystick=nullptr;
    if(id) SDL_DetachVirtualJoystick(id);
    id=0;
}
}
