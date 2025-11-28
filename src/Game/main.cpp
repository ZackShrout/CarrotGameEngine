#include <cstdint>
#include <cstdio>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include "Engine/Core/Platform/Wayland/WaylandWindow.h"

int main()
{
    constexpr uint32_t k_width{ 1280 };
    constexpr uint32_t k_height{ 720 };

    carrot::platform::wayland_window window(k_width, k_height, "Carrot Engine – Month 0");

    if (!window.get_wl_display() || !window.get_wl_surface()) {
        fprintf(stderr, "Failed to create Wayland window/surface\n");
        return 1;
    }

    // Instance
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Carrot";
    app_info.apiVersion = VK_API_VERSION_1_3;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = 2;
    create_info.ppEnabledExtensionNames = extensions;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return 1;
    }

    // Surface
    VkWaylandSurfaceCreateInfoKHR surface_info{};
    surface_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surface_info.display = window.get_wl_display();
    surface_info.surface = window.get_wl_surface();

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWaylandSurfaceKHR(instance, &surface_info, nullptr, &surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Wayland surface\n");
        return 1;
    }

    printf("Carrot Engine: Wayland window + Vulkan surface created successfully!\n");

    while (!window.should_close()) {
        window.poll_events();
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    return 0;
}