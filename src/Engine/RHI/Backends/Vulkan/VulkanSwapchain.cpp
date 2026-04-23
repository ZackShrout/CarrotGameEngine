//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanSwapchain.h"

#include "VulkanDevice.h"

namespace carrot::rhi::vulkan {
    namespace {
        [[nodiscard]] VkSurfaceFormatKHR choose_surface_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
        {
            uint32_t format_count{ 0 };
            VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));

            CE_ASSERT(format_count > 0 && "Surface must advertise at least one swapchain format");

            std::vector<VkSurfaceFormatKHR> formats(format_count);
            VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceFormatsKHR(
                physical_device,
                surface,
                &format_count,
                formats.data()
            ));

            for (const VkSurfaceFormatKHR& format : formats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return format;
                }
            }

            return formats.front();
        }

        [[nodiscard]] VkPresentModeKHR choose_present_mode(const VkPhysicalDevice physical_device,
                                                           const VkSurfaceKHR surface,
                                                           const bool present_sync_enabled)
        {
            uint32_t present_mode_count{ 0 };
            VK_CHECK_FATAL(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,
                                                                     surface,
                                                                     &present_mode_count,
                                                                     nullptr));

            CE_ASSERT(present_mode_count > 0 && "Surface must advertise at least one present mode");

            std::vector<VkPresentModeKHR> present_modes(present_mode_count);
            VK_CHECK_FATAL(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,
                                                                     surface,
                                                                     &present_mode_count,
                                                                     present_modes.data()));

            const auto supports = [&present_modes](const VkPresentModeKHR mode) noexcept
            {
                return std::ranges::find(present_modes, mode) != present_modes.end();
            };

            if (present_sync_enabled)
                return VK_PRESENT_MODE_FIFO_KHR;

            if (supports(VK_PRESENT_MODE_IMMEDIATE_KHR))
                return VK_PRESENT_MODE_IMMEDIATE_KHR;

            if (supports(VK_PRESENT_MODE_MAILBOX_KHR))
                return VK_PRESENT_MODE_MAILBOX_KHR;

            return VK_PRESENT_MODE_FIFO_KHR;
        }
    } // anonymous namespace

    vulkan_swapchain_t::vulkan_swapchain_t(vulkan_device_t* device, VkSurfaceKHR surface,
                                           const uint32_t width, const uint32_t height,
                                           VkSwapchainKHR old_swapchain/* = VK_NULL_HANDLE*/,
                                           const bool present_sync_enabled)
        : _device{ device }, _surface{ surface }, _present_sync_enabled{ present_sync_enabled }
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

        VkSurfaceCapabilitiesKHR caps{ };
        VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device->physical_device(), _surface, &caps));

        // Wayland can transiently report 0x0 currentExtent during async fullscreen/configure transitions.
        // Defer recreation until compositor reports a valid extent.
        if (caps.currentExtent.width != UINT32_MAX &&
            (caps.currentExtent.width == 0 || caps.currentExtent.height == 0))
        {
            LOG_GRAPHICS_WARN("Deferring swapchain resize while compositor extent is {}x{}",
                              caps.currentExtent.width,
                              caps.currentExtent.height);
            return;
        }

        // Store old swapchain handle for handover
        VkSwapchainKHR old_swapchain{ _swapchain.swapchain };

        // Clean up current resources (views, but not the swapchain itself yet)
        _image_views.reset();
        _images.clear();

        // Re-create the swapchain (pass the old one for efficient handover)
        create_or_recreate(old_swapchain, width, height);
    }

    void vulkan_swapchain_t::recreate()
    {
        // Query current surface capabilities (compositor tells us the truth)
        VkSurfaceCapabilitiesKHR caps{ };
        VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _device->physical_device(),
            _surface,
            &caps
        ));

        uint32_t width{ 0 };
        uint32_t height{ 0 };

        if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.width != 0 && caps.currentExtent.height != 0)
        {
            width = caps.currentExtent.width;
            height = caps.currentExtent.height;
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

    void vulkan_swapchain_t::present([[maybe_unused]] rhi_semaphore_t* wait_semaphore) {}

    void vulkan_swapchain_t::create_or_recreate(VkSwapchainKHR old_swapchain, uint32_t width, uint32_t height)
    {
        VkDevice device = _device->vk_device();
        VkPhysicalDevice phys = _device->physical_device();
        const VkSurfaceFormatKHR surface_format{ choose_surface_format(phys, _surface) };
        const VkPresentModeKHR present_mode{ choose_present_mode(phys, _surface, _present_sync_enabled) };

        VkSurfaceCapabilitiesKHR caps{ };
        VK_CHECK_FATAL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, _surface, &caps));

        VkExtent2D extent{ width, height };
        if (caps.currentExtent.width != ~0u &&
            caps.currentExtent.width != 0 &&
            caps.currentExtent.height != 0)
            extent = caps.currentExtent;
        else
        {
            extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        CE_ASSERT(extent.width > 0 && extent.height > 0 && "Swapchain extent must be non-zero");

        uint32_t image_count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0)
            image_count = chlm::min(image_count, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{ };
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = _surface;
        info.minImageCount = image_count;
        info.imageFormat = surface_format.format;
        info.imageColorSpace = surface_format.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = present_mode;
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
            view_info.format = surface_format.format;
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

        _format = surface_format.format;
        _extent = extent;
        _image_count = image_count;
        _present_mode = present_mode;

        const char* present_mode_name{
            present_mode == VK_PRESENT_MODE_FIFO_KHR ? "FIFO"
            : (present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX"
                                                           : (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE"
                                                                                                            : "OTHER"))
        };

        LOG_GRAPHICS_INFO("Swapchain created with an image count of {} and extent {}x{} (present mode: {}, present sync: {}).",
                          _image_count,
                          _extent.width,
                          _extent.height,
                          present_mode_name,
                          _present_sync_enabled ? "enabled" : "disabled");
    }
} // namespace carrot::rhi::vulkan
