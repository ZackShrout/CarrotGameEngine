//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Buffer.h"
#include "Texture.h"

#include <memory>
#include <string_view>

namespace carrot::renderer {
    class renderer_t;
}

namespace carrot::rhi {
    class rhi_command_queue_t;
    class rhi_swapchain_t;
    class rhi_device_t;

    enum class graphics_api { vulkan, direct_x12, metal, default_api, count };

    [[nodiscard]] static std::string_view graphics_api_to_string(const graphics_api api) noexcept
    {
        switch (api)
        {
            case graphics_api::vulkan: return "Vulkan";
            case graphics_api::direct_x12: return "DirectX12";
            case graphics_api::metal: return "Metal";
            case graphics_api::default_api: return "Platform Default";
            default: return "Unknown";
        }
    }

    struct rhi_desc_t
    {
        graphics_api api{ graphics_api::default_api };
        uint32_t width{ 1280 };
        uint32_t height{ 720 };
        bool enable_debug_layers{ true };

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

        [[nodiscard]] virtual std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) = 0;

        virtual void wait_idle() = 0;
    };

    // Factory
    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc);
} // namespace carrot::rhi
