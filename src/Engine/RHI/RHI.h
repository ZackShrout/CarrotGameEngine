//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Buffer.h"
#include "Renderer/Draw/TexturedQuadBatch.h"
#include "Renderer/Draw/TexturedQuadCameraUniform.h"
#include "Sampler.h"
#include "Texture.h"
#include "Window/Window.h"

#include <chlm/CarrotHLM.h>
#include <memory>
#include <span>
#include <string_view>

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

    struct textured_quad_stage_record_t
    {
        const rhi_buffer_t* vertex_buffer{ nullptr };
        const rhi_buffer_t* index_buffer{ nullptr };
        std::span<const renderer::textured_quad_batch_t> batches{ };
        chlm::float4x4 view_projection{ chlm::float4x4::identity() };
        chlm::float4 ambient_color{ 1.f, 1.f, 1.f, 1.f };
        chlm::float4 forward_plus_grid_params{ 0.f, 0.f, static_cast<float>(renderer::k_forward_plus_tile_size_px), 0.f };
        std::array<std::uint32_t, 4> forward_plus_tile_counts{ 0u, 0u, 0u, 0u };
        std::uint32_t point_light_count{ 0u };
        std::array<renderer::world_point_light_uniform_t, renderer::k_max_world_point_lights> point_lights{ };
        std::array<renderer::forward_plus_tile_header_t, renderer::k_max_forward_plus_tiles> forward_plus_tiles{ };
        std::array<renderer::packed_uint4_t, renderer::k_max_forward_plus_packed_light_index_words> forward_plus_light_indices{ };
        render_viewport_t viewport{ };
        uint32_t presentation_mask{ 1u };
    };

    constexpr uint32_t k_max_textured_quad_stage_slots_per_frame{ 16u };

    enum presentation_channel_bits_t : uint32_t
    {
        presentation_channel_gameplay = 1u << 0u,
        presentation_channel_log_console = 1u << 1u,
        presentation_channel_all = 0xFFFFFFFFu
    };

    [[nodiscard]] constexpr bool presentation_mask_includes(const uint32_t mask, const uint32_t channel) noexcept
    {
        return (mask & channel) != 0u;
    }

    enum class graphics_api { vulkan, direct_x12, metal, null_backend, default_api, count };

    [[nodiscard]] inline std::string_view graphics_api_to_string(const graphics_api api) noexcept
    {
        switch (api)
        {
            case graphics_api::vulkan: return "Vulkan";
            case graphics_api::direct_x12: return "DirectX12";
            case graphics_api::metal: return "Metal";
            case graphics_api::null_backend: return "Null";
            case graphics_api::default_api: return "Platform Default";
            default: return "Unknown";
        }
    }

    struct rhi_desc_t
    {
        graphics_api api{ graphics_api::default_api };
        window::window_id_t presentation_window_id{ window::invalid_window_id };
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
        virtual void record_textured_quad_stage(const textured_quad_stage_record_t& stage) = 0;
        virtual void record_text_quad_stage(const textured_quad_stage_record_t& stage) = 0;
        virtual void end_frame() = 0;

        virtual void release_asset_references() = 0;

        virtual void resize(uint32_t width, uint32_t height) = 0;

        [[nodiscard]] virtual rhi_device_t* get_device() const noexcept = 0;
        [[nodiscard]] virtual rhi_swapchain_t* get_swapchain() const noexcept = 0;
        [[nodiscard]] virtual rhi_command_queue_t* get_command_queue() const noexcept = 0;
        [[nodiscard]] virtual graphics_api get_graphics_api() const noexcept = 0;

        [[nodiscard]] virtual std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const = 0;

        [[nodiscard]] virtual rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) = 0;
        virtual void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) = 0;

        // Multi-window presentation surface management.
        // Backends can override these when they support more than one presentation surface.
        virtual bool add_presentation_window([[maybe_unused]] window::window_id_t window_id,
                                             [[maybe_unused]] uint32_t presentation_channel_mask =
                                                 presentation_channel_gameplay)
        {
            return false;
        }
        virtual bool remove_presentation_window([[maybe_unused]] window::window_id_t window_id) { return false; }

        virtual void wait_idle() = 0;
    };

    // Factory
    std::unique_ptr<rhi_context_t> create_rhi_context(const rhi_desc_t& desc);
} // namespace carrot::rhi
