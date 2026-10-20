// SpeedBreaker One. GPL-3.0-or-later.
#include "gpu_driver.h"
#include <adrenotools/driver.h>
#include <jni.h>
#include <dlfcn.h>
#include <cstdio>
#include <string>

namespace platform::android {
namespace {
void* s_loader = nullptr;
bool s_custom = false;
std::string JavaString(const char* methodName) {
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return {};
    auto type = env->GetObjectClass(activity);
    auto method = env->GetMethodID(type, methodName, "()Ljava/lang/String;");
    auto result = method ? static_cast<jstring>(env->CallObjectMethod(activity, method)) : nullptr;
    std::string value;
    if (env->ExceptionCheck()) env->ExceptionClear();
    else if (result) {
        const char* chars = env->GetStringUTFChars(result, nullptr);
        if (chars) { value = chars; env->ReleaseStringUTFChars(result, chars); }
    }
    if (result) env->DeleteLocalRef(result);
    env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
    return value;
}
}
bool InitializeVulkanLoader() {
    if (s_loader) return true;
    auto directory = JavaString("gpuDriverDirectory");
    auto library = JavaString("gpuDriverLibrary");
    auto hooks = JavaString("gpuHookDirectory");
    auto name = JavaString("gpuDriverName");
    if (!directory.empty() && !library.empty() && !hooks.empty()) {
        fprintf(stderr, "[driver] requesting custom driver: %s (%s)\n", name.c_str(), library.c_str());
        s_loader = adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM,
            nullptr, hooks.c_str(), directory.c_str(), library.c_str(), nullptr, nullptr);
        s_custom = s_loader != nullptr;
        if (!s_loader) fprintf(stderr, "[driver] custom loader failed; falling back to system driver\n");
    }
    if (!s_loader) s_loader = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!s_loader) { fprintf(stderr, "[driver] Vulkan loader unavailable: %s\n", dlerror()); return false; }
    auto getProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(s_loader, "vkGetInstanceProcAddr"));
    if (!getProc && s_custom) {
        fprintf(stderr, "[driver] custom loader has no entry point; falling back to system\n");
        s_loader = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        getProc = s_loader ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(s_loader, "vkGetInstanceProcAddr")) : nullptr;
        s_custom = false;
    }
    if (!getProc) { fprintf(stderr, "[driver] loader has no vkGetInstanceProcAddr\n"); return false; }
    volkInitializeCustom(getProc);
    if (s_custom) {
        // A valid hook handle can still silently expose the OEM driver. Probe
        // before creating any application handles and verify Turnip's identity.
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "SpeedBreaker driver probe"; app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; create.pApplicationInfo = &app;
        VkInstance probe = VK_NULL_HANDLE;
        bool usable = false;
        if (vkCreateInstance && vkCreateInstance(&create, nullptr, &probe) == VK_SUCCESS) {
            volkLoadInstance(probe);
            uint32_t count = 0;
            if (vkEnumeratePhysicalDevices(probe, &count, nullptr) == VK_SUCCESS && count > 0 && count <= 32) {
                VkPhysicalDevice devices[32];
                if (vkEnumeratePhysicalDevices(probe, &count, devices) == VK_SUCCESS) {
                    for (uint32_t i=0; i<count; ++i) {
                        VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
                        VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
                        props.pNext = &driver; vkGetPhysicalDeviceProperties2(devices[i], &props);
                        usable |= driver.driverID == VK_DRIVER_ID_MESA_TURNIP;
                    }
                }
            }
            vkDestroyInstance(probe, nullptr);
        }
        if (!usable) {
            fprintf(stderr, "[driver] custom driver probe failed or did not expose Turnip; falling back to system\n");
            // Keep hook namespaces resident. No custom handles survive the probe.
            s_loader = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
            getProc = s_loader ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(s_loader, "vkGetInstanceProcAddr")) : nullptr;
            if (!getProc) return false;
            s_custom = false; volkInitializeCustom(getProc);
        }
    }
    fprintf(stderr, "[driver] dispatch initialized: %s\n", s_custom ? "custom loader (hardware verification follows)" : "system");
    return vkCreateInstance && vkEnumerateInstanceExtensionProperties;
}
bool CreateVulkanSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface) {
    auto* nativeWindow = static_cast<ANativeWindow*>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr));
    if (!nativeWindow || !vkCreateAndroidSurfaceKHR) return false;
    VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    info.window = nativeWindow;
    VkResult result = vkCreateAndroidSurfaceKHR(instance, &info, nullptr, surface);
    if (result != VK_SUCCESS) fprintf(stderr, "[driver] Android surface creation failed: %d\n", int(result));
    return result == VK_SUCCESS;
}
void ReportVulkanDriver(VkPhysicalDevice device) {
    VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    props.pNext = &driver; vkGetPhysicalDeviceProperties2(device, &props);
    fprintf(stderr, "[driver] actual device=%s vendor=%04x driverID=%u name=%s info=%s requested_custom=%d\n",
        props.properties.deviceName, props.properties.vendorID, driver.driverID, driver.driverName, driver.driverInfo, s_custom);
    if (s_custom && driver.driverID != VK_DRIVER_ID_MESA_TURNIP)
        fprintf(stderr, "[driver] WARNING: requested custom loader did not identify Mesa Turnip; use launch selector to restore System if needed\n");
}
}
