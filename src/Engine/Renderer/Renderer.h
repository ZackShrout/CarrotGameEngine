//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Shaders/VFSShaderFileProvider.h"
#include "Camera/Camera2D.h"
#include "Core/Module.h"
#include "Draw/TexturedQuadBatch.h"
#include "Draw/TexturedQuadTypes.h"
#include "EngineConfig.h"
#include "Primitives/QuadVertex.h"
#include "RHI/RHI.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_buffer_t;
}

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::world {
    class world_t;
    struct layering_debug_snapshot_t;
}

namespace carrot::renderer {
    constexpr std::size_t k_textured_quad_frame_buffer_count{ 3 };

    struct sprite_draw_info_t;

    enum class frame_stage_kind_t : std::uint8_t
    {
        world = 0,
        ui,
        composite,
        overlay_debug,
        count
    };

    enum class frame_stage_space_t : std::uint8_t
    {
        world_camera = 0,
        viewport_pixels,
        render_target_pixels
    };

    struct tilemap_draw_info_t
    {
        const assets::loaded_tilemap_asset_t* tilemap{ nullptr };
        chlm::float2 origin{ 0.f, 0.f };
        chlm::float2 scale{ 1.f, 1.f };
        float source_pixels_per_unit{ 0.f };
        float render_pixels_per_unit{ 0.f };
        bool include_object_layers{ true };
        std::span<const std::string_view> active_visibility_tags{ };
        render_layer_t layer{ render_layer_t::world_back };
        render_order_mode_t order_mode{ render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
        uint32_t color{ 0xFFFFFFFFu };
    };

    struct solid_quad_draw_info_t
    {
        float x{ 0.f };
        float y{ 0.f };
        float width{ 1.f };
        float height{ 1.f };
        render_layer_t layer{ render_layer_t::debug };
        render_order_mode_t order_mode{ render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        uint32_t color{ 0xFFFFFFFFu };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
    };

    struct renderer_stats_t
    {
        uint32_t draw_calls{ 0 };
        uint32_t textured_quad_count{ 0 };
        uint32_t textured_quad_batch_count{ 0 };
        uint32_t vertex_count{ 0 };
        uint32_t index_count{ 0 };
    };

    struct textured_quad_state_t
    {
        struct frame_buffers_t
        {
            std::unique_ptr<rhi::rhi_buffer_t> vertex_buffer;
            std::unique_ptr<rhi::rhi_buffer_t> index_buffer;
            size_t vertex_capacity{ 0 };
            size_t index_capacity{ 0 };
        };

        struct submission_t
        {
            textured_quad_draw_info_t quad;
            uint64_t submission_index{ 0 };
        };

        std::vector<submission_t> submissions;

        // CPU-side submission/batching for the current frame
        std::vector<quad_vertex_t> vertices_cpu;
        std::vector<uint32_t> indices_cpu;
        std::vector<textured_quad_batch_t> batches;

        // Per-frame reusable geometry buffers
        std::array<frame_buffers_t, k_textured_quad_frame_buffer_count> frame_buffers;
    };

    class renderer_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("Renderer")

        explicit renderer_t(io::virtual_file_system_t& vfs, const engine_graphics_config_t& config);
        ~renderer_t() override { shutdown(); }

        void init() override;
        void shutdown() override;

        // Main loop integration
        void begin_frame();
        void end_frame();

        void set_camera_2d(const camera_2d_t& camera) noexcept { _active_camera = camera; }

        // Very high-level drawing commands (immediate mode for now)
        void draw_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_overlay_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_ui_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_overlay_solid_quad(const solid_quad_draw_info_t& quad);
        void draw_ui_solid_quad(const solid_quad_draw_info_t& quad);
        void set_fullscreen_overlay_color(uint32_t color_abgr) noexcept;
        void clear_fullscreen_overlay() noexcept;
        void draw_sprite(const sprite_draw_info_t& info);
        void draw_tilemap(const tilemap_draw_info_t& info);
        void draw_world(const world::world_t& world);

        // Hot-reload & debug support
        void notify_shader_changed(std::string_view path);

        // Basic stats & introspection
        [[nodiscard]] const renderer_stats_t& get_stats() const noexcept { return _stats; }
        [[nodiscard]] const renderer_stats_t& get_last_completed_stats() const noexcept { return _last_completed_stats; }
        [[nodiscard]] uint64_t get_frame_index() const noexcept { return _frame_index; }
        [[nodiscard]] rhi::rhi_context_t* get_rhi() const noexcept { return _rhi.get(); }
        [[nodiscard]] rhi::graphics_api get_graphics_api() const noexcept
        {
            return _rhi ? _rhi->get_graphics_api() : _config.api;
        }
        [[nodiscard]] const camera_2d_t& get_camera_2d() const noexcept { return _active_camera; }
        [[nodiscard]] resolved_camera_2d_t resolve_camera_2d() const noexcept
        {
            return _active_camera.resolve(current_render_target_size());
        }

    private:
        struct stage_execution_context_t
        {
            chlm::float4x4 view_projection{ chlm::float4x4::identity() };
            rhi::render_viewport_t viewport{ };
        };

        struct frame_stage_plan_t
        {
            frame_stage_kind_t kind{ frame_stage_kind_t::world };
            frame_stage_space_t space{ frame_stage_space_t::world_camera };
        };

        [[nodiscard]] chlm::uint2 current_render_target_size() const noexcept;
        [[nodiscard]] stage_execution_context_t resolve_stage_execution_context(const frame_stage_plan_t& stage_plan) const noexcept;
        void queue_fullscreen_overlay_if_needed();
        void submit_textured_quad(frame_stage_kind_t stage, const textured_quad_draw_info_t& quad);
        void submit_solid_quad(frame_stage_kind_t stage, const solid_quad_draw_info_t& quad);
        void build_textured_quad_batches(textured_quad_state_t& state) const;
        void execute_frame_stage(const frame_stage_plan_t& stage_plan);
        void execute_frame_stages();
        void release_frame_resources();
        void ensure_textured_quad_frame_buffers(textured_quad_state_t& state);
        void upload_textured_quad_frame_data(const textured_quad_state_t& state) const;
        void submit_tilemap(const tilemap_draw_info_t& info, world::layering_debug_snapshot_t* layering_debug_snapshot = nullptr);
        void submit_tile_object(const assets::loaded_tilemap_asset_t& tilemap,
                                uint32_t gid,
                                const chlm::float2& position_px,
                                const chlm::float2& size_px,
                                render_layer_t layer,
                                render_order_mode_t order_mode,
                                int32_t order_in_layer,
                                float sort_reference_y,
                                quad_sampler_preset_t sampler_preset,
                                uint32_t color);
        [[nodiscard]] uint32_t current_textured_quad_frame_buffer_slot() const noexcept;
        [[nodiscard]] textured_quad_state_t::frame_buffers_t& current_frame_buffers(textured_quad_state_t& state) const noexcept;
        [[nodiscard]] const textured_quad_state_t::frame_buffers_t& current_frame_buffers(const textured_quad_state_t& state) const noexcept;

        // ── External context / configuration ──────────────────────────────────────
        io::virtual_file_system_t&  _vfs;
        engine_graphics_config_t    _config;

        // ── Backend integration ───────────────────────────────────────────────────
        std::unique_ptr<rhi::rhi_context_t>                 _rhi;
        std::unique_ptr<assets::vfs_shader_file_provider_t> _shader_provider;

        // ── Frame progression / stats ─────────────────────────────────────────────
        uint64_t            _frame_index{ 0 };
        renderer_stats_t    _stats;
        renderer_stats_t    _last_completed_stats;
        uint64_t            _animated_tiles_elapsed_ms{ 0 };
        std::chrono::steady_clock::time_point _animated_tiles_clock_origin{ std::chrono::steady_clock::now() };

        // ── Renderer path state: textured quads ───────────────────────────────────
        std::array<textured_quad_state_t, static_cast<size_t>(frame_stage_kind_t::count)> _stage_textured_quads;
        std::array<frame_stage_plan_t, static_cast<size_t>(frame_stage_kind_t::count)> _frame_stage_plan{
            frame_stage_plan_t{ .kind = frame_stage_kind_t::world, .space = frame_stage_space_t::world_camera },
            frame_stage_plan_t{ .kind = frame_stage_kind_t::ui, .space = frame_stage_space_t::render_target_pixels },
            frame_stage_plan_t{ .kind = frame_stage_kind_t::composite, .space = frame_stage_space_t::render_target_pixels },
            frame_stage_plan_t{ .kind = frame_stage_kind_t::overlay_debug, .space = frame_stage_space_t::viewport_pixels }
        };

        camera_2d_t _active_camera{ };
        std::unique_ptr<rhi::rhi_texture_t> _solid_white_texture;
        bool _fullscreen_overlay_enabled{ false };
        uint32_t _fullscreen_overlay_color{ 0x00000000u };
    };
} // namespace carrot::renderer
