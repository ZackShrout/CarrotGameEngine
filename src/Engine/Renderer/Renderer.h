//
// Created by zshrout on 1/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Module.h"
#include "RHI/RHI.h"

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

        renderer_t() { init(); }
        ~renderer_t() override { shutdown(); }

        void init() override;
        void shutdown() override;

        // Main loop integration
        void begin_frame();
        void end_frame();

        // Very high-level drawing commands (immediate mode for now)
        void draw_fullscreen_colored_triangle(uint32_t abgr_color = 0xFF00FFFF);
        void draw_fullscreen_quad(const fullscreen_quad_info_t& info = { });
        void draw_sprite(const sprite_draw_info_t& sprite);

        // Hot-reload & debug support
        void notify_shader_changed(std::string_view path);

        // Basic stats & introspection
        [[nodiscard]] uint64_t get_frame_index() const noexcept { return _frame_index; }
        [[nodiscard]] uint32_t get_draw_call_count() const noexcept { return _draw_calls_this_frame; }
        [[nodiscard]] rhi::rhi_context_t* get_rhi() const noexcept { return _rhi.get(); }

    private:
        std::unique_ptr<rhi::rhi_context_t> _rhi;

        uint64_t _frame_index{ 0 };
        uint32_t _draw_calls_this_frame{ 0 };

        // Future common resources (starting small)
        // rhi::rhi_graphics_pipeline_t* _colored_triangle_pipeline{ nullptr };
        // rhi::rhi_graphics_pipeline_t* _textured_quad_pipeline{ nullptr };
        // rhi::rhi_texture_t* _default_white_tex{ nullptr };

        void create_common_resources();
        void destroy_common_resources();

        // Temporary bridge helpers until we have proper command list abstraction
        void submit_immediate_triangle(uint32_t abgr_color);
    };
} // namespace carrot::renderer
