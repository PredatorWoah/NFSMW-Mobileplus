// SpeedBreaker. GPL-3.0-or-later. SAF -> seekable fd -> existing XDVDFS parser.
#include "disc_picker.h"
#include <install/installer.h>
#include <ui/ui.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <jni.h>
#include <future>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
    std::mutex s_mutex;
    bool s_waiting = false, s_answered = false;
    int s_fd = -1;
    std::string s_name, s_error;

    std::string String(JNIEnv* env, jstring text)
    {
        if (!text) return {};
        const char* chars = env->GetStringUTFChars(text, nullptr);
        if (!chars) return {};
        std::string result(chars);
        env->ReleaseStringUTFChars(text, chars);
        return result;
    }
    int JavaInt(const char* name)
    {
        auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (!env || !activity) return -1;
        auto type = env->GetObjectClass(activity);
        auto method = env->GetMethodID(type, name, "()I");
        int result = method ? env->CallIntMethod(activity, method) : -1;
        if (env->ExceptionCheck()) { env->ExceptionClear(); result = -1; }
        env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
        return result;
    }
    bool JavaVoid(const char* name)
    {
        auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (!env || !activity) return false;
        auto type = env->GetObjectClass(activity);
        auto method = env->GetMethodID(type, name, "()V");
        if (method) env->CallVoidMethod(activity, method);
        bool ok = method && !env->ExceptionCheck();
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
        return ok;
    }
    std::string JavaName()
    {
        auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (!env || !activity) return "Disc image";
        auto type = env->GetObjectClass(activity);
        auto method = env->GetMethodID(type, "discName", "()Ljava/lang/String;");
        auto name = method ? static_cast<jstring>(env->CallObjectMethod(activity, method)) : nullptr;
        std::string result = String(env, name);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (name) env->DeleteLocalRef(name);
        env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
        return result;
    }
    struct Checked
    {
        std::optional<platform::android::SelectedDisc> disc;
        std::string error;
    };
    Checked Check(int fd, std::string name)
    {
        struct Close { int fd; ~Close() { close(fd); } } owner{fd};
        try
        {
            struct stat st{};
            if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || lseek(fd, 0, SEEK_CUR) < 0)
                return {{}, "This provider does not offer a seekable local file. Choose the ISO on internal storage or the SD card."};
            std::unique_ptr<install::DiscSource> source;
            auto result = install::OpenDiscImageDescriptor(fd, name, source);
            if (!result.Ok()) return {{}, result.message};
            result = install::CheckSource(*source);
            if (!result.Ok()) return {{}, result.message};
            auto* xex = source->Find("default.xex");
            if (!xex || xex->size != install::GameManifest().Xex().size)
                return {{}, "The disc executable is missing or has an unsupported size."};
            platform::android::SelectedDisc disc;
            disc.name = std::move(name);
            disc.xex.resize(size_t(xex->size));
            std::string error;
            auto reader = source->Open(*xex, error);
            if (!reader || !reader->Read(0, disc.xex.data(), disc.xex.size(), error)) return {{}, error};
            disc.source = std::move(source);
            fprintf(stderr, "[disc] validated %s: %zu files, direct ISO reads; no extraction\n",
                disc.name.c_str(), disc.source->Files().size());
            return {std::move(disc), {}};
        }
        catch (const std::exception& e) { return {{}, e.what()}; }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_onenonlygit_speedbreakerone_SpeedBreakerActivity_nativeDiscResult(JNIEnv* env, jclass, jint fd, jstring name, jstring error)
{
    std::lock_guard lock(s_mutex);
    if (!s_waiting) { if (fd >= 0) close(fd); return; }
    s_waiting = false; s_answered = true; s_fd = fd;
    s_name = String(env, name); s_error = String(env, error);
}

namespace platform::android
{
    void RememberDisc() { JavaVoid("rememberDisc"); }
    std::optional<SelectedDisc> ChooseDisc(const std::function<bool()>& runFrame)
    {
        std::future<Checked> checking;
        int saved = JavaInt("openSavedDisc");
        if (saved >= 0) checking = std::async(std::launch::async, Check, saved, JavaName());
        std::optional<SelectedDisc> selected;
        std::string error;
        bool quit = false;
        ui::SetModal([&] {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(.5f, .5f));
            ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_Always);
            ImGui::Begin("SpeedBreaker One", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextWrapped("Choose your Xbox 360 Most Wanted ISO. The game reads it in place; no extraction or second ISO copy is needed.");
            if (!error.empty()) { ImGui::Spacing(); ImGui::TextWrapped("%s", error.c_str()); }
            bool waiting;
            { std::lock_guard lock(s_mutex); waiting = s_waiting; }
            bool busy = waiting || checking.valid();
            if (checking.valid()) ImGui::TextUnformatted("Checking the game version and disc file list...");
            ImGui::Spacing();
            ImGui::BeginDisabled(busy);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            if (ImGui::Button("Choose ISO", ImVec2(180, 48)))
            {
                { std::lock_guard lock(s_mutex); s_waiting = true; s_answered = false; }
                if (!JavaVoid("pickDisc"))
                { std::lock_guard lock(s_mutex); s_waiting = false; error = "Could not open the Android file picker."; }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Exit", ImVec2(100, 48))) quit = true;
            ImGui::End();
        });
        while (!quit && !selected)
        {
            if (!runFrame()) break;
            {
                std::lock_guard lock(s_mutex);
                if (s_answered)
                {
                    s_answered = false;
                    error = std::move(s_error);
                    if (s_fd >= 0)
                    {
                        checking = std::async(std::launch::async, Check, s_fd, std::move(s_name));
                        s_fd = -1;
                    }
                }
            }
            if (checking.valid() && checking.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                auto result = checking.get();
                selected = std::move(result.disc);
                error = std::move(result.error);
                if (!selected) { fprintf(stderr, "[disc] rejected: %s\n", error.c_str()); JavaVoid("abandonDisc"); }
            }
        }
        ui::SetModal(nullptr);
        { std::lock_guard lock(s_mutex); s_waiting = false; if (s_fd >= 0) close(s_fd); s_fd = -1; s_answered = false; }
        if (!selected) JavaVoid("abandonDisc");
        return selected;
    }
}
