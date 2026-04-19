//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    class renderer_t;
    struct solid_quad_draw_info_t;
}

namespace carrot::world {
    class world_t;
}

namespace carrot::core {
    struct game_view_camera_t
    {
        float zoom{ 1.f };
        chlm::float2 center_world{ 0.f, 0.f };
    };

    class game_view_t
    {
    public:
        explicit game_view_t(renderer::renderer_t& renderer) noexcept
            : _renderer{ renderer } {}

        [[nodiscard]] float camera_zoom() const noexcept;
        void set_camera_zoom(float zoom) noexcept;
        [[nodiscard]] chlm::float2 camera_center_world_position(const world::world_t& world) const noexcept;
        void center_camera_on_world_position(const world::world_t& world, const chlm::float2& world_position) noexcept;
        [[nodiscard]] game_view_camera_t camera_state(const world::world_t& world) const noexcept;
        void set_camera_state(const world::world_t& world, const game_view_camera_t& state) noexcept;
        void set_composite_overlay_color(uint32_t color_abgr) noexcept;
        void clear_composite_overlay() noexcept;
        void draw_composite_solid_quad(float x, float y, float width, float height, uint32_t color_abgr) noexcept;
        void draw_overlay_solid_quad(float x, float y, float width, float height, uint32_t color_abgr) noexcept;
        [[nodiscard]] chlm::uint2 render_target_pixel_size() const noexcept;

    private:
        renderer::renderer_t& _renderer;
    };
} // namespace carrot::core
