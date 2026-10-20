// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
namespace platform::android {
// Call before any Vulkan entry point. The module remains loaded for process life.
bool InitializeVulkanLoader();
bool CreateVulkanSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface);
void ReportVulkanDriver(VkPhysicalDevice device);
}
