#include "Engine/RHI/VulkanContext.h"
#include <vector>
#include <cstdio>
#include <algorithm>

namespace carrot::rhi {

static uint32_t find_queue_family(VkPhysicalDevice phys, VkSurfaceKHR surface, VkQueueFlags flags)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if ((families[i].queueFlags & flags) == flags) {
            if (surface) {
                VkBool32 supported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &supported);
                if (supported) return i;
            } else {
                return i;
            }
        }
    }
    return ~0u;
}

void vulkan_context::init(VkInstance inst, VkSurfaceKHR surf)
{
    instance = inst;
    surface = surf;

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    physical_device = devices[0];

    graphics_family = find_queue_family(physical_device, surface, VK_QUEUE_GRAPHICS_BIT);
    present_family = graphics_family;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = graphics_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;

    const char* device_ext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_ext;

    vkCreateDevice(physical_device, &device_info, nullptr, &device);
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    present_queue = graphics_queue;
}

void vulkan_context::create_swapchain(uint32_t width, uint32_t height)
{
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

    swapchain_extent = { width, height };
    if (caps.currentExtent.width != ~0u) swapchain_extent = caps.currentExtent;

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) image_count = std::min(image_count, caps.maxImageCount);

    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface;
    info.minImageCount = image_count;
    info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    info.imageExtent = swapchain_extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    vkCreateSwapchainKHR(device, &info, nullptr, &swapchain);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    swapchain_images = new VkImage[image_count];
    swapchain_views = new VkImageView[image_count];
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images);

    for (uint32_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &view_info, nullptr, &swapchain_views[i]);
    }
    this->image_count = image_count;
    swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
}

void vulkan_context::cleanup()
{
    for (uint32_t i = 0; i < image_count; ++i)
        vkDestroyImageView(device, swapchain_views[i], nullptr);
    delete[] swapchain_images;
    delete[] swapchain_views;
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    if (device) vkDestroyDevice(device, nullptr);
}

} // namespace carrot::rhi
