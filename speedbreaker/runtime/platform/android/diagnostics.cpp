// SpeedBreaker One. GPL-3.0-or-later. Game-free platform probe only.
#include "physical_memory.h"
#include "storage.h"
#include "../atomic_ref.h"
#include <atomic>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <android/log.h>
#include <jni.h>
#include <vulkan/vulkan.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
namespace {
FILE* logFile = nullptr;
std::string results;
void Log(const char* fmt, ...) {
    char text[2048]; va_list ap; va_start(ap, fmt); vsnprintf(text, sizeof(text), fmt, ap); va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "SpeedBreakerOne", text);
    if (logFile) { fprintf(logFile, "%s\n", text); fflush(logFile); }
}
void Check(bool ok, const char* name) {
    Log("[%s] %s", ok ? "PASS" : "FAIL", name);
    results += std::string(ok ? "PASS: " : "FAIL: ") + name + "\n";
}
uint8_t* probe = nullptr;
volatile sig_atomic_t faultCount = 0;
void Fault(int sig, siginfo_t* info, void*) {
    auto p = reinterpret_cast<uintptr_t>(info->si_addr);
    auto start = reinterpret_cast<uintptr_t>(probe);
    if (probe && p >= start && p < start + 4096 && mprotect(probe, 4096, PROT_READ | PROT_WRITE) == 0) {
        faultCount = faultCount + 1; return;
    }
    _exit(128 + sig);
}
bool AtomicProbe() {
    alignas(4) uint32_t word = 1;
    platform::AtomicRef ref(word);
    std::atomic<bool> entered{false}, resumed{false};
    std::thread waiter([&] { entered.store(true); ref.wait(1); resumed.store(ref.load() == 0); });
    while (!entered.load()) std::this_thread::yield();
    ref.store(0); ref.notify_one(); waiter.join();
    uint32_t expected = 0;
    bool cas = ref.compare_exchange_strong(expected, 7);
    expected = 2;
    bool mismatch = !ref.compare_exchange_strong(expected, 8) && expected == 7;
    return resumed.load() && cas && mismatch;
}
bool MemoryProbe() {
    constexpr size_t size = 0x100000000ULL;
    auto* base = static_cast<uint8_t*>(mmap(reinterpret_cast<void*>(size), size,
        PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    Check(base != MAP_FAILED, "4 GiB guest reservation");
    if (base == MAP_FAILED) return false;
    uint32_t shift = 0;
    bool mapped = platform::android::MapPhysicalMemory(base, shift);
    Check(mapped, "512 MiB shared physical backing / three windows");
    if (!mapped) { munmap(base, size); return false; }
    auto* a = reinterpret_cast<volatile uint32_t*>(base + 0xA0001000ULL);
    auto* c = reinterpret_cast<volatile uint32_t*>(base + 0xC0001000ULL);
    auto* e = reinterpret_cast<volatile uint32_t*>(base + 0xE0000000ULL);
    *a = 0x13579bdf;
    bool aliases = *c == *a && *e == *a && shift == 4096;
    *e = 0x2468ace0;
    aliases = aliases && *a == *e && *c == *e;
    Check(aliases, "bidirectional A/C/E aliases and exact +4 KiB E shift");
    struct sigaction sa{}, old{}; sa.sa_sigaction = Fault; sa.sa_flags = SA_SIGINFO; sigemptyset(&sa.sa_mask);
    bool signals = sigaction(SIGSEGV, &sa, &old) == 0;
    probe = base + 0xA0001000ULL;
    bool protectedWrite = signals && mprotect(probe, 4096, PROT_READ) == 0;
    if (protectedWrite) *a = 0xfeed1234;
    bool protectedRead = protectedWrite && mprotect(probe, 4096, PROT_NONE) == 0;
    volatile uint32_t value = protectedRead ? *a : 0;
    bool protection = protectedRead && faultCount == 2 && value == 0xfeed1234 && *c == value && *e == value;
    if (signals) sigaction(SIGSEGV, &old, nullptr);
    probe = nullptr;
    Check(protection, "mprotect read/write faults, SIGSEGV retry, alias visibility");
    // This probes kernel primitives, not the entire runtime write-watch algorithm.
    munmap(base, size);
    return aliases && protection;
}
bool VulkanProbe(SDL_Window* window) {
    Uint32 extCount = 0;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!extensions) { Log("SDL Vulkan extensions: %s", SDL_GetError()); return false; }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = extCount; ci.ppEnabledExtensionNames = extensions;
    VkInstance instance = VK_NULL_HANDLE; VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool ok = vkCreateInstance(&ci, nullptr, &instance) == VK_SUCCESS;
    Check(ok, "Vulkan 1.2 instance"); if (!ok) return false;
    ok = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
    Check(ok, "Android Vulkan surface via SDL3");
    if (ok) {
        uint32_t count = 0; vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data());
        bool found = false;
        for (auto d : devices) {
            VkPhysicalDeviceProperties p{}; vkGetPhysicalDeviceProperties(d, &p);
            Log("GPU=%s vendor=0x%04x device=0x%04x driver=%u API=%u.%u.%u", p.deviceName, p.vendorID, p.deviceID, p.driverVersion,
                VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion), VK_VERSION_PATCH(p.apiVersion));
            Log("limits: descriptor sets=%u push constants=%u timestamp period=%g ns", p.limits.maxBoundDescriptorSets,
                p.limits.maxPushConstantsSize, p.limits.timestampPeriod);
            uint32_t n = 0; vkGetPhysicalDeviceQueueFamilyProperties(d, &n, nullptr);
            std::vector<VkQueueFamilyProperties> queues(n); vkGetPhysicalDeviceQueueFamilyProperties(d, &n, queues.data());
            uint32_t presentQueue = UINT32_MAX;
            for (uint32_t q = 0; q < n; q++) { VkBool32 present = false; vkGetPhysicalDeviceSurfaceSupportKHR(d, q, surface, &present);
                Log("queue=%u flags=0x%x present=%u timestamp bits=%u", q, queues[q].queueFlags, present, queues[q].timestampValidBits);
                if (presentQueue == UINT32_MAX && present && (queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) presentQueue = q; }
            VkPhysicalDeviceFeatures f{}; vkGetPhysicalDeviceFeatures(d, &f);
            Log("BC compression=%u shaderInt16=%u dualSrcBlend=%u", f.textureCompressionBC, f.shaderInt16, f.dualSrcBlend);
            Log("features: vertex stores/atomics=%u fragment stores/atomics=%u independent blend=%u depth clamp=%u anisotropy=%u precise occlusion=%u",
                f.vertexPipelineStoresAndAtomics, f.fragmentStoresAndAtomics, f.independentBlend,
                f.depthClamp, f.samplerAnisotropy, f.occlusionQueryPrecise);
            VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
            VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; f2.pNext = &dynamic;
            vkGetPhysicalDeviceFeatures2(d, &f2);
            Check(dynamic.dynamicRendering, "dynamicRendering feature");
            bool bc = f.textureCompressionBC;
            for (VkFormat format : {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK,
                    VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK}) {
                VkFormatProperties fp{}; vkGetPhysicalDeviceFormatProperties(d, format, &fp);
                VkImageFormatProperties image{};
                VkResult supported = vkGetPhysicalDeviceImageFormatProperties(d, format, VK_IMAGE_TYPE_2D,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &image);
                bool usable = supported == VK_SUCCESS && (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
                    (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) &&
                    (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
                bc &= usable;
                Log("BC format=%d sampled=%u transfer-dst=%u linear-filter=%u image-query=%d extent=%ux%u layers=%u",
                    int(format), bool(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT),
                    bool(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT),
                    bool(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
                    int(supported), image.maxExtent.width, image.maxExtent.height, image.maxArrayLayers);
            }
            Check(bc, "BC1/2/3/5 sampled/transfer/linear image support (capability queries only)");
            uint32_t en = 0; vkEnumerateDeviceExtensionProperties(d, nullptr, &en, nullptr);
            std::vector<VkExtensionProperties> available(en); vkEnumerateDeviceExtensionProperties(d, nullptr, &en, available.data());
            // Keep this list aligned with presenter.cpp::kNeededDeviceExtensions.
            const char* requiredExtensions[] = {"VK_KHR_swapchain", "VK_KHR_dynamic_rendering", "VK_KHR_depth_stencil_resolve", "VK_KHR_create_renderpass2", "VK_KHR_multiview", "VK_KHR_maintenance2", "VK_KHR_push_descriptor"};
            bool allExtensions = true;
            for (const char* required : requiredExtensions) {
                bool has = false; for (const auto& x : available) has |= strcmp(x.extensionName, required) == 0;
                Log("renderer extension %s: %s", required, has ? "yes" : "MISSING");
                allExtensions &= has;
            }
            Check(allExtensions, "all seven SpeedBreaker renderer extensions");
            if (allExtensions && dynamic.dynamicRendering && presentQueue != UINT32_MAX) {
                float priority = 1.0f;
                VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                qi.queueFamilyIndex = presentQueue; qi.queueCount = 1; qi.pQueuePriorities = &priority;
                VkPhysicalDeviceFeatures enabled{};
                enabled.vertexPipelineStoresAndAtomics = f.vertexPipelineStoresAndAtomics;
                enabled.fragmentStoresAndAtomics = f.fragmentStoresAndAtomics;
                enabled.independentBlend = f.independentBlend; enabled.dualSrcBlend = f.dualSrcBlend;
                enabled.depthClamp = f.depthClamp; enabled.samplerAnisotropy = f.samplerAnisotropy;
                enabled.textureCompressionBC = f.textureCompressionBC; enabled.shaderInt16 = f.shaderInt16;
                VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.pNext = &dynamic;
                di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi; di.pEnabledFeatures = &enabled;
                di.enabledExtensionCount = uint32_t(sizeof(requiredExtensions) / sizeof(requiredExtensions[0]));
                di.ppEnabledExtensionNames = requiredExtensions;
                VkDevice device = VK_NULL_HANDLE; VkResult result = vkCreateDevice(d, &di, nullptr, &device);
                Log("vkCreateDevice with baseline renderer features=%d", int(result));
                Check(result == VK_SUCCESS, "logical device with baseline renderer features");
                found |= result == VK_SUCCESS;
                if (device) vkDestroyDevice(device, nullptr);
            }
        }
        Check(found, "present-capable SpeedBreaker logical device"); ok = found;
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    vkDestroyInstance(instance, nullptr); return ok;
}
}
int main(int, char**) {
    auto file = platform::android::Files() / "android-diagnostics.log";
    logFile = fopen(file.c_str(), "w");
    Log("SpeedBreaker One Vivo USA bring-up platform diagnostics; NO GAME CODE; page size=%ld", sysconf(_SC_PAGESIZE));
    Check(sysconf(_SC_PAGESIZE) == 4096, "4096-byte pages required by current exact E alias backend");
    struct sysinfo info{}; if (sysinfo(&info) == 0) Log("RAM total=%llu free=%llu bytes", (unsigned long long)info.totalram * info.mem_unit, (unsigned long long)info.freeram * info.mem_unit);
    Check(MemoryProbe(), "guest-memory primitive probe complete");
    Check(AtomicProbe(), "guest word atomic CAS / futex wait and notify");
    bool sdl = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO);
    Check(sdl, "SDL video/gamepad/audio initialization");
    if (!sdl) { Log("SDL error: %s", SDL_GetError()); if (logFile) fclose(logFile); return 1; }
    SDL_Window* window = SDL_CreateWindow("SpeedBreaker One — diagnostics", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
    Check(window != nullptr, "fullscreen Vulkan window");
    if (window) Check(VulkanProbe(window), "Vulkan surface capability probe complete");
    int count = 0; SDL_JoystickID* ids = SDL_GetGamepads(&count);
    std::vector<SDL_Gamepad*> pads;
    for (int i = 0; i < count; i++) { auto* pad = SDL_OpenGamepad(ids[i]); if (pad) { pads.push_back(pad); Log("Controller: %s", SDL_GetGamepadName(pad)); } }
    SDL_free(ids); int audioCount = 0; auto* audioIds = SDL_GetAudioPlaybackDevices(&audioCount);
    Log("gamepads=%zu; audio playback devices=%d", pads.size(), audioCount); SDL_free(audioIds);
    results += "\nThis is a platform probe, not the playable game.\nResults are in android-diagnostics.log and logcat tag SpeedBreakerOne.\nClose this dialog, then press buttons to log controller input.\nAndroid Back exits.";
    // SDL's simple Android message box puts unscrollable text above its buttons.
    // This report can exceed a landscape phone's height, making dismissal
    // unreachable. Use the probe's scrollable dialog with fixed action buttons.
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    auto cls = env->GetObjectClass(activity);
    auto method = env->GetMethodID(cls, "showProbeSummary", "(Ljava/lang/String;)V");
    auto summary = env->NewStringUTF(results.c_str());
    env->CallVoidMethod(activity, method, summary);
    env->DeleteLocalRef(summary);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    bool running = true;
    while (running && window) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch(e.type) {
            case SDL_EVENT_QUIT: case SDL_EVENT_TERMINATING: running = false; break;
            case SDL_EVENT_KEY_DOWN: if (e.key.key == SDLK_AC_BACK) running = false; break;
            case SDL_EVENT_GAMEPAD_ADDED: { auto* pad = SDL_OpenGamepad(e.gdevice.which); if (pad) { pads.push_back(pad); Log("Controller added: %s", SDL_GetGamepadName(pad)); } break; }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: Log("button gamepad=%u button=%u", e.gbutton.which, e.gbutton.button); break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: Log("axis gamepad=%u axis=%u value=%d", e.gaxis.which, e.gaxis.axis, e.gaxis.value); break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND: Log("lifecycle: background"); break;
            case SDL_EVENT_DID_ENTER_FOREGROUND: Log("lifecycle: foreground"); break;
            default: break;
            }
        }
        SDL_Delay(20);
    }
    for (auto* p : pads) SDL_CloseGamepad(p);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); if (logFile) fclose(logFile); return 0;
}
