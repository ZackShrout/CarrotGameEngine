//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "VulkanSwapchain.h"

#include "VulkanDevice.h"

namespace carrot::rhi::vulkan {
    vulkan_swapchain_t::vulkan_swapchain_t(vulkan_device_t* device, VkSurfaceKHR surface,
                                           [[maybe_unused]] void* native_window,
                                           const uint32_t width, const uint32_t height,
                                           VkSwapchainKHR old_swapchain/* = VK_NULL_HANDLE*/)
        : _device{ device }, _surface{ surface }
    {
        create_or_recreate(old_swapchain, width, height);
    }

    vulkan_swapchain_t::~vulkan_swapchain_t()
    {
        _image_views.reset();
        _swapchain = { };
    }

    void vulkan_swapchain_t::resize(uint32_t width, uint32_t height)
    {
        // Prevent invalid/zero size (common during minimize)
        if (width == 0 || height == 0)
        {
            LOG_GRAPHICS_WARN("Resize to zero size ignored (window minimized?)");
            return;
        }

        // Wait for GPU to finish using the current swapchain
        vkDeviceWaitIdle(_device->vk_device());

        // Store old swapchain handle for handover
        VkSwapchainKHR old_swapchain{ _swapchain.swapchain };

        // Clean up current resources (views, but not the swapchain itself yet)
        _image_views.reset();
        _images.clear();

        // Re-create the swapchain (pass the old one for efficient handover)
        create_or_recreate(old_swapchain, width, height);

        // If you have framebuffers stored in the swapchain class, recreate them here too
        // (most engines keep framebuffers in the renderer/context, so you may skip this)
        // _framebuffers = create_framebuffers(_render_pass);  // if you moved them here

        LOG_GRAPHICS_INFO("Swapchain resized to {}x{}", width, height);
    }

    void vulkan_swapchain_t::recreate()
    {
        // Safety wait
        vkDeviceWaitIdle(_device->vk_device());

        // Query current surface capabilities (compositor tells us the truth)
        VkSurfaceCapabilitiesKHR caps{ };
        VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _device->physical_device(),
            _surface,
            &caps
        ));

        uint32_t width{ 0 };
        uint32_t height{ 0 };

        if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.width != 0)
        {
            width = caps.currentExtent.width;
            height = caps.currentExtent.height;
            LOG_GRAPHICS_INFO("Using compositor-provided extent for recreation: {}x{}", width, height);
        }
        else
        {
            // Very rare fallback — compositor allows us to choose
            // Use current extent if we have one, or a safe default
            width = _extent.width ? _extent.width : 1280;
            height = _extent.height ? _extent.height : 720;
            LOG_GRAPHICS_WARN("No currentExtent from compositor — using fallback {}x{}", width, height);
        }

        if (width == 0 || height == 0)
        {
            LOG_GRAPHICS_WARN("Zero extent detected — skipping recreation (likely minimized)");
            return;
        }

        // Now just call the existing resize() with the discovered size
        resize(width, height);
    }

    framebuffer_array_t vulkan_swapchain_t::create_framebuffers(VkRenderPass render_pass) const
    {
        framebuffer_array_t framebuffers{ _device->vk_device() };
        framebuffers.resize(_image_views.size());

        for (size_t i{ 0 }; i < _image_views.size(); ++i)
        {
            VkFramebufferCreateInfo fb_info{ };
            fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_info.renderPass = render_pass;
            fb_info.attachmentCount = 1;
            fb_info.pAttachments = &_image_views[i];
            fb_info.width = _extent.width;
            fb_info.height = _extent.height;
            fb_info.layers = 1;

            VK_CHECK_FATAL(vkCreateFramebuffer(
                _device->vk_device(),
                &fb_info,
                nullptr,
                &framebuffers[i]
            ));
        }

        LOG_GRAPHICS_INFO("Created {} framebuffers for swapchain", framebuffers.size());
        return framebuffers;
    }

    uint32_t vulkan_swapchain_t::acquire_next_image(rhi_semaphore_t* signal_semaphore)
    {
        VkSemaphore vk_sem{ signal_semaphore ? /* cast later */ VK_NULL_HANDLE : VK_NULL_HANDLE };

        uint32_t image_index;
        const VkResult result{
            vkAcquireNextImageKHR(_device->vk_device(), _swapchain.swapchain, ~0ULL,
                                  vk_sem, VK_NULL_HANDLE, &image_index)
        };

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            // Trigger resize/recreate
            return ~0u; // special value meaning "needs recreate"
        }

        VK_CHECK_FATAL(result);
        _current_image_index = image_index;
        return image_index;
    }

    void vulkan_swapchain_t::present(rhi_semaphore_t* wait_semaphore) {}

    void vulkan_swapchain_t::create_or_recreate(VkSwapchainKHR old_swapchain, uint32_t width, uint32_t height)
    {
        VkDevice device = _device->vk_device();
        VkPhysicalDevice phys = _device->physical_device();

        VkSurfaceCapabilitiesKHR caps{ };
        VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, _surface, &caps));

        VkExtent2D extent{ width, height };
        if (caps.currentExtent.width != ~0u)
            extent = caps.currentExtent;

        uint32_t image_count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0)
            image_count = std::min(image_count, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{ };
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = _surface;
        info.minImageCount = image_count;
        info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;

        VkSwapchainKHR new_swapchain;
        VK_CHECK_FATAL(vkCreateSwapchainKHR(device, &info, nullptr, &new_swapchain));

        // Clean up old first if recreating
        if (old_swapchain != VK_NULL_HANDLE)
        {
            _image_views.reset();
            _swapchain = { };
        }

        _swapchain = swapchain_t{ device, new_swapchain };

        VK_CHECK_FATAL(vkGetSwapchainImagesKHR(device, _swapchain, &image_count, nullptr));
        _images.resize(image_count);
        VK_CHECK_FATAL(vkGetSwapchainImagesKHR(device, _swapchain, &image_count, _images.data()));

        _image_views = image_view_array_t{ device };
        _image_views.resize(image_count);

        for (uint32_t i = 0; i < image_count; ++i)
        {
            VkImageViewCreateInfo view_info{ };
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = _images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = VK_FORMAT_B8G8R8A8_SRGB;
            view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;

            VkImageView view;
            VK_CHECK_FATAL(vkCreateImageView(device, &view_info, nullptr, &view));
            _image_views[i] = view;
        }

        _format = VK_FORMAT_B8G8R8A8_SRGB;
        _extent = extent;
        _image_count = image_count;

        LOG_GRAPHICS_INFO("Swapchain created with an image count of {}.", _image_count);
    }
} // namespace carrot::rhi::vulkan
