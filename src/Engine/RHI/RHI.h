//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Buffer.h"
#include "Sampler.h"
#include "Texture.h"
#include "Renderer/Draw/TexturedQuadBatch.h"

#include <memory>
#include <span>
#include <string_view>
#include <chlm/CarrotHLM.h>

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::renderer {
    class renderer_t;
}

namespace carrot::rhi {
    class rhi_command_queue_t;
    class rhi_swapchain_t;
    class rhi_device_t;

    struct render_viewport_t
    {
        chlm::uint_rect rect_px{
            .position = { 0u, 0u },
            .size = { 1u, 1u }
        };
    };

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
        assets::shader_file_provider_t* shader_files{ nullptr };
    };

    class rhi_context_t
    {
    public:
        virtual ~rhi_context_t() = default;

        virtual void begin_frame() = 0;
        virtual void record_frame() = 0;
        virtual void end_frame() = 0;

        virtual void release_asset_references() = 0;

        virtual void resize(uint32_t width, uint32_t height) = 0;

        [[nodiscard]] virtual rhi_device_t* get_device() const noexcept = 0;
        [[nodiscard]] virtual rhi_swapchain_t* get_swapchain() const noexcept = 0;
        [[nodiscard]] virtual rhi_command_queue_t* get_command_queue() const noexcept = 0;

        [[nodiscard]] virtual std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const = 0;
        virtual void set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer, const rhi_buffer_t& index_buffer) = 0;
        virtual void set_textured_quad_batches(std::span<const renderer::textured_quad_batch_t> batches) = 0;
        virtual void set_textured_quad_view_projection(const chlm::float4x4& view_projection) = 0;
        virtual void set_textured_quad_viewport(const render_viewport_t& viewport) = 0;

        [[nodiscard]] virtual rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) = 0;
        virtual void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) = 0;

        virtual void wait_idle() = 0;
    };

    // Factory
    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc);
} // namespace carrot::rhi
