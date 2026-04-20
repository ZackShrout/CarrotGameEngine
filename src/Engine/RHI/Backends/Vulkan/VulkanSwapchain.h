//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Swapchain.h"
#include "VulkanCommon.h"
#include "VulkanCore.h"

namespace carrot::rhi::vulkan {
    class vulkan_device_t;

    class vulkan_swapchain_t final : public rhi_swapchain_t
    {
    public:
        vulkan_swapchain_t(vulkan_device_t* device, VkSurfaceKHR surface, uint32_t width,
                           uint32_t height, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
        ~vulkan_swapchain_t() override;

        void resize(uint32_t width, uint32_t height) override;
        void recreate();

        [[nodiscard]] framebuffer_array_t create_framebuffers(VkRenderPass render_pass) const;

        uint32_t acquire_next_image(rhi_semaphore_t* signal_semaphore) override;
        void present(rhi_semaphore_t* wait_semaphore) override;

        [[nodiscard]] rhi_texture_t* get_current_backbuffer() const override { return nullptr; } // later
        [[nodiscard]] uint32_t get_current_image_index() const override { return _current_image_index; }
        [[nodiscard]] uint32_t get_image_count() const override { return _image_count; }
        [[nodiscard]] uint32_t get_width() const override { return _extent.width; }
        [[nodiscard]] uint32_t get_height() const override { return _extent.height; }

        [[nodiscard]] VkSwapchainKHR vk_swapchain() const noexcept { return _swapchain.swapchain; }
        [[nodiscard]] VkFormat format() const noexcept { return _format; }
        [[nodiscard]] VkExtent2D extent() const { return _extent; }
        [[nodiscard]] VkImage image(const uint32_t index) const noexcept { return _images[index]; }

    private:
        void create_or_recreate(VkSwapchainKHR old_swapchain, uint32_t width, uint32_t height);

        vulkan_device_t*        _device{ nullptr };
        VkSurfaceKHR            _surface{ VK_NULL_HANDLE };
        swapchain_t             _swapchain;
        image_view_array_t      _image_views;
        std::vector<VkImage>    _images;

        VkFormat                _format{ VK_FORMAT_UNDEFINED };
        VkExtent2D              _extent{ };
        uint32_t                _image_count{ 0 };
        uint32_t                _current_image_index{ 0 };
    };
} // namespace carrot::rhi::vulkan
