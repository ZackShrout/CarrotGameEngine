//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#include "VulkanContext.h"

#include "Common/CommonHeaders.h"

#include <vector>
#include <algorithm>

namespace carrot::rhi::vulkan {
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

    vulkan_context_t* vulkan_context_t::_context{ nullptr };

    void vulkan_context_t::init(VkInstance inst, VkSurfaceKHR surf)
    {
        _instance = inst;
        _surface = surf;

        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(_instance, &device_count, devices.data());
        _physical_device = devices[0];

        _graphics_family = find_queue_family(_physical_device, _surface, VK_QUEUE_GRAPHICS_BIT);
        _present_family = _graphics_family;

        constexpr float priority{ 1.0f };
        VkDeviceQueueCreateInfo queue_info{ };
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = _graphics_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;

        const char* device_ext[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo device_info{ };
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = 1;
        device_info.ppEnabledExtensionNames = device_ext;

        vkCreateDevice(_physical_device, &device_info, nullptr, &_device);
        vkGetDeviceQueue(_device, _graphics_family, 0, &_graphics_queue);
        _present_queue = _graphics_queue;

        VkCommandPoolCreateInfo transient_pool_info{};
        transient_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        transient_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // optional but useful
        transient_pool_info.queueFamilyIndex = _graphics_family;

        vkCreateCommandPool(_device, &transient_pool_info, nullptr, &_transient_command_pool);

        _context = this;
    }

    void vulkan_context_t::create_swapchain(const uint32_t width, const uint32_t height)
    {
        VkSurfaceCapabilitiesKHR caps{ };
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical_device, _surface, &caps);

        _swapchain_extent = { width, height };
        if (caps.currentExtent.width != ~0u) _swapchain_extent = caps.currentExtent;

        uint32_t img_count{ caps.minImageCount + 1 };
        if (caps.maxImageCount > 0) img_count = std::min(img_count, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{ };
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = _surface;
        info.minImageCount = img_count;
        info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        info.imageExtent = _swapchain_extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = VK_NULL_HANDLE;

        vkCreateSwapchainKHR(_device, &info, nullptr, &_swapchain);
        vkGetSwapchainImagesKHR(_device, _swapchain, &img_count, nullptr);
        _swapchain_images = new VkImage[img_count];
        _swapchain_views = new VkImageView[img_count];
        vkGetSwapchainImagesKHR(_device, _swapchain, &img_count, _swapchain_images);

        for (uint32_t i = 0; i < img_count; ++i)
        {
            VkImageViewCreateInfo view_info{ };
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = _swapchain_images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
            view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            vkCreateImageView(_device, &view_info, nullptr, &_swapchain_views[i]);
        }
        _image_count = img_count;
        _swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
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

        vkCreateBuffer(_device, &buffer_info, nullptr, &buffer);

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(_device, buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info{ };
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

        vkAllocateMemory(_device, &alloc_info, nullptr, &memory);
        vkBindBufferMemory(_device, buffer, memory, 0);
    }

    void vulkan_context_t::cleanup()
    {
        for (uint32_t i = 0; i < _image_count; ++i)
            vkDestroyImageView(_device, _swapchain_views[i], nullptr);
        delete[] _swapchain_images;
        delete[] _swapchain_views;
        if (_swapchain) vkDestroySwapchainKHR(_device, _swapchain, nullptr);

        if (_transient_command_pool)
        {
            vkDestroyCommandPool(_device, _transient_command_pool, nullptr);
            _transient_command_pool = VK_NULL_HANDLE;
        }

        if (_device) vkDestroyDevice(_device, nullptr);

        _context = nullptr;
    }

    uint32_t vulkan_context_t::find_memory_type(const uint32_t type_filter, VkMemoryPropertyFlags properties) const noexcept
    {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(_physical_device, &mem_props);

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
        alloc_info.commandPool = _transient_command_pool;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(_device, &alloc_info, &cmd);

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

        vkQueueSubmit(_graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(_graphics_queue);

        vkFreeCommandBuffers(_device, _transient_command_pool, 1, &cmd);
    }
} // namespace carrot::rhi::vulkan
