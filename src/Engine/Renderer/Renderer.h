//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "EngineConfig.h"
#include "Assets/Shaders/VFSShaderFileProvider.h"
#include "Core/Module.h"
#include "Draw/TexturedQuadBatch.h"
#include "Draw/TexturedQuadTypes.h"
#include "Primitives/QuadVertex.h"
#include "RHI/RHI.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "Camera/Camera2D.h"

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_buffer_t;
}

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::renderer {
    struct sprite_draw_info_t;

    struct tilemap_draw_info_t
    {
        const assets::loaded_tilemap_asset_t* tilemap{ nullptr };
        chlm::float2 origin{ 0.f, 0.f };
        render_layer_t layer{ render_layer_t::world_back };
        int32_t order_in_layer{ 0 };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
        uint32_t color{ 0xFFFFFFFFu };
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

        // Reusable frame geometry buffers
        std::unique_ptr<rhi::rhi_buffer_t> frame_vertex_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> frame_index_buffer;

        // Current allocated capacities in bytes
        size_t vertex_capacity{ 0 };
        size_t index_capacity{ 0 };
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
        void draw_sprite(const sprite_draw_info_t& info);
        void draw_tilemap(const tilemap_draw_info_t& info);

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
        [[nodiscard]] chlm::uint2 current_render_target_size() const noexcept;
        void build_textured_quad_batches();
        void release_frame_resources();
        void ensure_textured_quad_frame_buffers();
        void upload_textured_quad_frame_data() const;

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

        // ── Renderer path state: textured quads ───────────────────────────────────
        textured_quad_state_t _textured_quad;

        camera_2d_t _active_camera{ };
    };
} // namespace carrot::renderer
