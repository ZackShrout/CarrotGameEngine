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
    class game_view_t
    {
    public:
        explicit game_view_t(renderer::renderer_t& renderer) noexcept
            : _renderer{ renderer } {}

        void set_zoom(float zoom) noexcept;
        void set_fullscreen_overlay_color(uint32_t color_abgr) noexcept;
        void clear_fullscreen_overlay() noexcept;
        void draw_overlay_solid_quad(float x, float y, float width, float height, uint32_t color_abgr) noexcept;
        [[nodiscard]] chlm::float2 center_world_position(const world::world_t& world) const noexcept;
        void set_center_world_position(const world::world_t& world, const chlm::float2& world_position) noexcept;

    private:
        renderer::renderer_t& _renderer;
    };
} // namespace carrot::core
