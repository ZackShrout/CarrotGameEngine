//
// Created by zshrout on 1/3/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Buffer.h"
#include "Pipeline.h"
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

    struct quad_stage_common_t
    {
        enum class target_load_action_t : std::uint8_t
        {
            clear = 0,
            load
        };

        chlm::float4x4 view_projection{ chlm::float4x4::identity() };
        chlm::float4 ambient_color{ 1.f, 1.f, 1.f, 1.f };
        renderer::forward_plus_frame_constants_t forward_plus_constants{ };
        renderer::forward_plus_light_input_t forward_plus_light_input{ };
        renderer::forward_plus_classification_output_t forward_plus_output{ };
        const rhi_buffer_t* forward_plus_light_input_buffer{ nullptr };
        const rhi_buffer_t* forward_plus_output_buffer{ nullptr };
        const rhi_buffer_t* world_item_buffer{ nullptr };
        const rhi_buffer_t* visible_item_index_buffer{ nullptr };
        std::uint32_t world_draw_mode{ 0u };
        render_viewport_t viewport{ };
        uint32_t presentation_mask{ 1u };
        const rhi_render_target_t* render_target{ nullptr };
        target_load_action_t target_load_action{ target_load_action_t::clear };
        chlm::float4 target_clear_color{ 0.f, 0.f, 0.f, 0.f };
    };

    enum class quad_draw_source_kind_t : std::uint8_t
    {
        direct = 0,
        indexed_indirect
    };

    enum class quad_shader_variant_t : std::uint8_t
    {
        standard = 0,
        battle_swirl,
        bloom_blur,
        bloom_composite
    };

    struct quad_draw_source_t
    {
        const rhi_buffer_t* vertex_buffer{ nullptr };
        const rhi_buffer_t* index_buffer{ nullptr };
        const rhi_buffer_t* instance_buffer{ nullptr };
        const rhi_buffer_t* indirect_buffer{ nullptr };
        std::uint32_t instance_count{ 0u };
        std::uint32_t indirect_buffer_offset_bytes{ 0u };
        quad_draw_source_kind_t kind{ quad_draw_source_kind_t::direct };
    };

    struct textured_quad_stage_record_t : quad_stage_common_t, quad_draw_source_t
    {
        std::span<const renderer::textured_quad_batch_t> batches{ };
        quad_shader_variant_t shader_variant{ quad_shader_variant_t::standard };
        bool capture_presentation_before_draw{ false };
    };

    struct indirect_textured_quad_stage_record_t : quad_stage_common_t, quad_draw_source_t
    {
        const rhi_texture_t* texture{ nullptr };
        const rhi_sampler_t* sampler{ nullptr };
    };

    // Shared renderer-facing RHI limit:
    // every native backend provisions per-frame textured/text stage resources
    // against this stage-slot budget, so changes here are parity-sensitive.
    //
    // The original 16-slot budget was fine for the earlier "a few broad stage
    // records per frame" renderer slices. Milestone 25's indirect world path
    // records one textured stage per validated world material/texture run, so
    // practical scenes need a meaningfully larger budget.
    constexpr uint32_t k_max_textured_quad_stage_slots_per_frame{ 1024u };

    enum presentation_channel_bits_t : uint32_t
    {
        presentation_channel_gameplay = 1u << 0u,
        presentation_channel_log_console = 1u << 1u,
        presentation_channel_all = 0xFFFFFFFFu
    };

    // Current shared presentation routing contract for the practical renderer slice.
    constexpr uint32_t k_known_presentation_channel_mask{
        presentation_channel_gameplay | presentation_channel_log_console
    };

    [[nodiscard]] constexpr bool presentation_mask_includes(const uint32_t mask, const uint32_t channel) noexcept
    {
        return (mask & channel) != 0u;
    }

    [[nodiscard]] constexpr bool presentation_mask_uses_known_channels(const uint32_t mask) noexcept
    {
        return (mask & ~k_known_presentation_channel_mask) == 0u;
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

        /**
         * Practical live renderer contract:
         * - frame lifecycle (`begin_frame`, `record_*_stage`, `end_frame`)
         * - dynamic texture/buffer/sampler creation through the context itself
         * - sampler binding/cache support for textured quad pipelines
         * - resize / auxiliary presentation window management
         * - explicit `wait_idle` for shutdown or resource replacement boundaries
         *
         * The engine's current renderer path is context-centric. `rhi_device_t`
         * remains available as a backend-owned low-level object, but it is not
         * a shared factory contract for the live renderer slice.
         */
        virtual void begin_frame() = 0;
        virtual void record_textured_quad_stage(const textured_quad_stage_record_t& stage) = 0;
        virtual void record_indirect_textured_quad_stage(const indirect_textured_quad_stage_record_t& stage) = 0;
        virtual void record_text_quad_stage(const textured_quad_stage_record_t& stage) = 0;
        virtual void end_frame() = 0;

        virtual void release_asset_references() = 0;

        virtual void resize(uint32_t width, uint32_t height) = 0;

        // Backend-owned low-level access point. The live renderer slice should
        // prefer the context-level resource and frame APIs above.
        [[nodiscard]] virtual rhi_device_t* get_device() const noexcept = 0;
        [[nodiscard]] virtual rhi_swapchain_t* get_swapchain() const noexcept = 0;
        [[nodiscard]] virtual rhi_command_queue_t* get_command_queue() const noexcept = 0;
        [[nodiscard]] virtual graphics_api get_graphics_api() const noexcept = 0;

        [[nodiscard]] virtual std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_render_target_t> create_render_target_2d(
            const render_target_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_compute_pipeline_t> create_compute_pipeline(
            const compute_pipeline_create_info_t& info) = 0;
        [[nodiscard]] virtual std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const = 0;

        [[nodiscard]] virtual rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) = 0;
        virtual void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) = 0;
        // Current explicit pass-boundary contract:
        // - the first live compute slice dispatches before graphics stage recording
        // - the caller must declare whether later graphics stages will read compute-written storage data
        // - backends own the native barriers/encoder boundaries needed to honor that shared declaration
        virtual void dispatch_compute(const compute_dispatch_record_t& record) = 0;

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
