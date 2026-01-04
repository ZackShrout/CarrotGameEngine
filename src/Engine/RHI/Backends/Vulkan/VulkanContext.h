//
// Created by zshrout on 11/27/25.
// Copyright (c) 2025 BunnySofty. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"

namespace carrot::rhi::vulkan {
    class vulkan_context_t
    {
    public:
        void init(VkInstance inst, VkSurfaceKHR surf);
        void create_swapchain(uint32_t width, uint32_t height);
        void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties,
                           VkBuffer& buffer, VkDeviceMemory& memory) const noexcept;
        void cleanup();

        [[nodiscard]] static vulkan_context_t* get() noexcept { return _context; }

        [[nodiscard]] uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const noexcept;
        [[nodiscard]] VkCommandBuffer begin_one_time_commands() const noexcept;
        void end_one_time_commands(VkCommandBuffer cmd) const noexcept;

        void set_render_pass(VkRenderPass render_pass) noexcept { _render_pass = render_pass; }

        [[nodiscard]] VkInstance instance() const noexcept { return _instance; }
        [[nodiscard]] VkDevice device() const noexcept { return _device; }
        [[nodiscard]] VkSurfaceKHR surface() const noexcept { return _surface; }
        [[nodiscard]] uint32_t graphics_family() const noexcept { return _graphics_family; }
        [[nodiscard]] VkCommandPool transient_command_pool() const noexcept { return _transient_command_pool; }
        [[nodiscard]] VkRenderPass render_pass() const noexcept { return _render_pass; }
        [[nodiscard]] VkQueue graphics_queue() const noexcept { return _graphics_queue; }
        [[nodiscard]] VkQueue present_queue() const noexcept { return _present_queue; }
        [[nodiscard]] VkSwapchainKHR* swapchain() noexcept { return &_swapchain; }
        [[nodiscard]] VkFormat swapchain_format() const noexcept { return _swapchain_format; }
        [[nodiscard]] VkExtent2D swapchain_extent() const noexcept { return _swapchain_extent; }
        [[nodiscard]] uint32_t image_count() const noexcept { return _image_count; }
        [[nodiscard]] VkImageView* swapchain_views() const noexcept { return _swapchain_views; }

    private:
        VkInstance          _instance{ VK_NULL_HANDLE };
        VkPhysicalDevice    _physical_device{ VK_NULL_HANDLE };
        VkDevice            _device{ VK_NULL_HANDLE };
        VkSurfaceKHR        _surface{ VK_NULL_HANDLE };
        VkCommandPool       _transient_command_pool{ VK_NULL_HANDLE };
        VkRenderPass        _render_pass{ VK_NULL_HANDLE };
        uint32_t            _graphics_family{ ~0u };
        uint32_t            _present_family{ ~0u };
        VkQueue             _graphics_queue{ VK_NULL_HANDLE };
        VkQueue             _present_queue{ VK_NULL_HANDLE };
        VkSwapchainKHR      _swapchain{ VK_NULL_HANDLE };
        VkFormat            _swapchain_format{ };
        VkExtent2D          _swapchain_extent{ };
        uint32_t            _image_count{ 0 };
        VkImage*            _swapchain_images{ nullptr };
        VkImageView*        _swapchain_views{ nullptr };

        static vulkan_context_t* _context;
    };
} // namespace carrot::rhi::vulkan
