//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameView.h"

#include "Renderer/Renderer.h"
#include "World/World.h"

namespace carrot::core {
    void game_view_t::set_zoom(const float zoom) noexcept
    {
        renderer::camera_2d_t camera{ _renderer.get_camera_2d() };
        camera.zoom = zoom > 0.f ? zoom : camera.zoom;
        _renderer.set_camera_2d(camera);
    }

    void game_view_t::set_fullscreen_overlay_color(const uint32_t color_abgr) noexcept
    {
        _renderer.set_fullscreen_overlay_color(color_abgr);
    }

    void game_view_t::clear_fullscreen_overlay() noexcept
    {
        _renderer.clear_fullscreen_overlay();
    }

    chlm::float2 game_view_t::center_world_position(const world::world_t& world) const noexcept
    {
        const renderer::camera_2d_t camera{ _renderer.get_camera_2d() };
        const chlm::float2 visible_world_size{ _renderer.resolve_camera_2d().visible_world_size };
        const chlm::float2 center_render_position{
            camera.position.x + (visible_world_size.x * 0.5f),
            camera.position.y + (visible_world_size.y * 0.5f)
        };
        return world.presentation().pixel_position_to_world(center_render_position);
    }

    void game_view_t::set_center_world_position(const world::world_t& world,
                                                const chlm::float2& world_position) noexcept
    {
        renderer::camera_2d_t camera{ _renderer.get_camera_2d() };
        const chlm::float2 render_position_px{ world.presentation().world_position_to_pixels(world_position) };
        const chlm::float2 visible_world_size{ _renderer.resolve_camera_2d().visible_world_size };

        camera.position = {
            render_position_px.x - visible_world_size.x * 0.5f, render_position_px.y - visible_world_size.y * 0.5f
        };

        _renderer.set_camera_2d(camera);
    }
} // namespace carrot::core
