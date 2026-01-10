//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <memory>

namespace carrot::renderer {
    class renderer_t;
}

namespace carrot::rhi {
    class rhi_command_queue_t;
    class rhi_swapchain_t;
    class rhi_device_t;

    enum class graphics_api { vulkan, direct_x12, metal, count };

    struct rhi_desc_t
    {
        graphics_api    api{ graphics_api::vulkan };
        void*           window_handle{ nullptr }; // wl_surface*, HWND, NSView*
        uint32_t        width{ 1280 };
        uint32_t        height{ 720 };
        bool            enable_debug_layers{ true };

        // Temporary bridge for migration
        renderer::renderer_t* existing_renderer = nullptr;
    };

    class rhi_context_t
    {
    public:
        virtual ~rhi_context_t() = default;

        virtual void begin_frame() = 0;
        virtual void record_frame() = 0;
        virtual void end_frame() = 0;

        virtual void resize(uint32_t width, uint32_t height) = 0;

        [[nodiscard]] virtual rhi_device_t* get_device() const noexcept = 0;
        [[nodiscard]] virtual rhi_swapchain_t* get_swapchain() const noexcept = 0;
        [[nodiscard]] virtual rhi_command_queue_t* get_command_queue() const noexcept = 0;

        virtual void wait_idle() = 0;
    };

    // Factory
    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc);
} // namespace carrot::rhi
