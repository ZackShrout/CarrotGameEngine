//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "VulkanContext.h"

#include "Engine/Common/CommonHeaders.h"

#include <vector>
#include <algorithm>

namespace carrot::rhi {
    static uint32_t find_queue_family(VkPhysicalDevice phys, VkSurfaceKHR surface, VkQueueFlags flags)
    {
        uint32_t count{ 0 };
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, families.data());

        for (uint32_t i = 0; i < count; ++i)
        {
            if ((families[i].queueFlags & flags) == flags)
            {
                if (surface)
                {
                    VkBool32 supported = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &supported);
                    if (supported) return i;
                }
                else
                {
                    return i;
                }
            }
        }
        return ~0u;
    }

    vulkan_context_t* vulkan_context_t::_instance{ nullptr };

    void vulkan_context_t::init(VkInstance inst, VkSurfaceKHR surf)
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

        constexpr float priority{ 1.0f };
        VkDeviceQueueCreateInfo queue_info{ };
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = graphics_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;

        const char* device_ext[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo device_info{ };
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = device_ext;

        vkCreateDevice(physical_device, &device_info, nullptr, &device);
        vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
        present_queue = graphics_queue;

        VkCommandPoolCreateInfo pool_info{ };
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = graphics_family;
        vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);

        _instance = this;
    }

    void vulkan_context_t::create_swapchain(const uint32_t width, const uint32_t height)
    {
        VkSurfaceCapabilitiesKHR caps{ };
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

        swapchain_extent = { width, height };
        if (caps.currentExtent.width != ~0u) swapchain_extent = caps.currentExtent;

        uint32_t img_count{ caps.minImageCount + 1 };
        if (caps.maxImageCount > 0) img_count = std::min(img_count, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{ };
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = img_count;
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
        vkGetSwapchainImagesKHR(device, swapchain, &img_count, nullptr);
        swapchain_images = new VkImage[img_count];
        swapchain_views = new VkImageView[img_count];
        vkGetSwapchainImagesKHR(device, swapchain, &img_count, swapchain_images);

        for (uint32_t i = 0; i < img_count; ++i)
        {
            VkImageViewCreateInfo view_info{ };
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = swapchain_images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
            view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            vkCreateImageView(device, &view_info, nullptr, &swapchain_views[i]);
        }
        image_count = img_count;
        swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    }

    void vulkan_context_t::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                         VkMemoryPropertyFlags properties,
                                         VkBuffer& buffer, VkDeviceMemory& memory) const noexcept
    {
        VkBufferCreateInfo buffer_info{ };
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &buffer_info, nullptr, &buffer);

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

        vkAllocateMemory(device, &alloc_info, nullptr, &memory);
        vkBindBufferMemory(device, buffer, memory, 0);
    }

    void vulkan_context_t::cleanup() const
    {
        for (uint32_t i = 0; i < image_count; ++i)
            vkDestroyImageView(device, swapchain_views[i], nullptr);
        delete[] swapchain_images;
        delete[] swapchain_views;
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        if (command_pool) vkDestroyCommandPool(device, command_pool, nullptr);
        if (device) vkDestroyDevice(device, nullptr);

        _instance = nullptr;
    }

    uint32_t vulkan_context_t::find_memory_type(const uint32_t type_filter, VkMemoryPropertyFlags properties) const noexcept
    {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
        {
            if (type_filter & 1u << i && (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        LOG_GRAPHICS_ERROR("[Vulkan] Failed to find suitable memory type!");
        return 0;
    }

    VkCommandBuffer vulkan_context_t::begin_one_time_commands() const noexcept
    {
        VkCommandBufferAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info{ };
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin_info);

        return cmd;
    }

    void vulkan_context_t::end_one_time_commands(VkCommandBuffer cmd) const noexcept
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{ };
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        vkQueueSubmit(graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_queue);

        vkFreeCommandBuffers(device, command_pool, 1, &cmd);
    }
} // namespace carrot::rhi
