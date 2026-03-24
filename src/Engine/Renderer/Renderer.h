//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "EngineConfig.h"
#include "Assets/Shaders/VFSShaderFileProvider.h"
#include "Core/Module.h"
#include "Draw/TexturedQuadBatch.h"
#include "Draw/TexturedQuadDrawInfo.h"
#include "Primitives/QuadVertex.h"
#include "RHI/RHI.h"

#include <vector>

namespace carrot::io {
    class virtual_file_system_t;
}

namespace carrot::rhi {
    class rhi_buffer_t;
    class rhi_texture_t;
}

namespace carrot::renderer {
    struct sprite_draw_info_t
    {
        float x{ 0.0f };
        float y{ 0.0f };
        float w{ 1.0f };
        float h{ 1.0f };
        uint32_t color{ 0xFFFFFFFF }; // ABGR - white by default
        // uint64_t texture_id{0};      // future
        // float rotation{0.0f};
        // vec2 pivot{0.5f, 0.5f};
    };

    struct fullscreen_quad_info_t
    {
        // Currently empty - future: tint, uv offset/scale, texture override, etc.
    };

    class renderer_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("Renderer")

        explicit renderer_t(io::virtual_file_system_t& vfs, const engine_graphics_config_t& config);
        ~renderer_t() override { shutdown(); }

        void init() override;
        void shutdown() override;

        void init_common_resources();

        // Main loop integration
        void begin_frame();
        void record_frame();
        void end_frame();

        // Very high-level drawing commands (immediate mode for now)
        void draw_fullscreen_colored_triangle(uint32_t abgr_color = 0xFF00FFFF);
        void draw_fullscreen_quad(const fullscreen_quad_info_t& info = { });
        void draw_textured_quad(const textured_quad_draw_info_t& quad);
        void draw_sprite(const sprite_draw_info_t& sprite);

        // Hot-reload & debug support
        void notify_shader_changed(std::string_view path);

        // Basic stats & introspection
        [[nodiscard]] uint64_t get_frame_index() const noexcept { return _frame_index; }
        [[nodiscard]] uint32_t get_draw_call_count() const noexcept { return _draw_calls_this_frame; }
        [[nodiscard]] rhi::rhi_context_t* get_rhi() const noexcept { return _rhi.get(); }

    private:
        void create_common_resources();
        void destroy_common_resources();

        // Temporary bridge helpers until we have proper command list abstraction
        void submit_immediate_triangle(uint32_t abgr_color);

        void ensure_textured_quad_frame_buffers();
        void upload_textured_quad_frame_data();

        io::virtual_file_system_t& _vfs;

        engine_graphics_config_t _config;

        std::unique_ptr<rhi::rhi_context_t> _rhi;
        std::unique_ptr<assets::vfs_shader_file_provider_t> _shader_provider;

        uint64_t _frame_index{ 0 };
        uint32_t _draw_calls_this_frame{ 0 };

        // std::unique_ptr<rhi::rhi_buffer_t> _quad_vertex_buffer;
        // std::unique_ptr<rhi::rhi_buffer_t> _quad_index_buffer;
        //
        // const rhi::rhi_texture_t* _test_texture{ nullptr };
        //
        // std::optional<textured_quad_draw_info_t> _pending_textured_quad;
        // const rhi::rhi_texture_t* _current_textured_quad_texture{ nullptr };

        std::unique_ptr<rhi::rhi_buffer_t> _quad_vertex_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> _quad_index_buffer;

        const rhi::rhi_texture_t* _test_texture{ nullptr };

        // Per-frame CPU-side textured quad batching
        std::vector<quad_vertex_t> _textured_quad_vertices_cpu;
        std::vector<uint32_t> _textured_quad_indices_cpu;
        std::vector<textured_quad_batch_t> _textured_quad_batches;

        std::unique_ptr<rhi::rhi_buffer_t> _textured_quad_frame_vertex_buffer;
        std::unique_ptr<rhi::rhi_buffer_t> _textured_quad_frame_index_buffer;

        size_t _textured_quad_vertex_capacity{ 0 };
        size_t _textured_quad_index_capacity{ 0 };
    };
} // namespace carrot::renderer
