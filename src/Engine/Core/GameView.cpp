//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameView.h"

#include "Renderer/Renderer.h"
#include "World/World.h"

namespace carrot::core {
    float game_view_t::camera_zoom() const noexcept
    {
        return _renderer.get_camera_2d().zoom;
    }

    void game_view_t::set_camera_zoom(const float zoom) noexcept
    {
        renderer::camera_2d_t camera{ _renderer.get_camera_2d() };
        camera.zoom = zoom > 0.f ? zoom : camera.zoom;
        _renderer.set_camera_2d(camera);
    }

    chlm::float2 game_view_t::camera_center_world_position(const world::world_t& world) const noexcept
    {
        const renderer::camera_2d_t camera{ _renderer.get_camera_2d() };
        const chlm::float2 visible_world_size{ _renderer.resolve_camera_2d().visible_world_size };
        const chlm::float2 center_render_position{
            camera.position.x + (visible_world_size.x * 0.5f),
            camera.position.y + (visible_world_size.y * 0.5f)
        };
        return world.presentation().pixel_position_to_world(center_render_position);
    }

    void game_view_t::center_camera_on_world_position(const world::world_t& world,
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

    game_view_camera_t game_view_t::camera_state(const world::world_t& world) const noexcept
    {
        return game_view_camera_t{
            .zoom = camera_zoom(),
            .center_world = camera_center_world_position(world)
        };
    }

    void game_view_t::set_camera_state(const world::world_t& world, const game_view_camera_t& state) noexcept
    {
        set_camera_zoom(state.zoom);
        center_camera_on_world_position(world, state.center_world);
    }

    void game_view_t::set_composite_overlay_color(const uint32_t color_abgr) noexcept
    {
        _renderer.set_composite_overlay_color(color_abgr);
    }

    void game_view_t::set_transition_fade_color(const uint32_t color_abgr) noexcept
    {
        _renderer.set_transition_fade_color(color_abgr);
    }

    void game_view_t::clear_transition_fade() noexcept
    {
        _renderer.clear_transition_fade();
    }

    void game_view_t::set_transition_battle_swirl(const float progress, const bool incoming) noexcept
    {
        _renderer.set_transition_battle_swirl(progress, incoming);
    }

    void game_view_t::clear_transition_battle_swirl() noexcept
    {
        _renderer.clear_transition_battle_swirl();
    }

    void game_view_t::clear_composite_overlay() noexcept
    {
        _renderer.clear_composite_overlay();
    }

    void game_view_t::draw_composite_solid_quad(const float x,
                                                const float y,
                                                const float width,
                                                const float height,
                                                const uint32_t color_abgr) noexcept
    {
        _renderer.draw_composite_solid_quad(renderer::solid_quad_draw_info_t{
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .layer = renderer::render_layer_t::ui,
            .color = color_abgr
        });
    }

    void game_view_t::draw_overlay_solid_quad(const float x,
                                              const float y,
                                              const float width,
                                              const float height,
                                              const uint32_t color_abgr) noexcept
    {
        _renderer.draw_overlay_solid_quad(renderer::solid_quad_draw_info_t{
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .color = color_abgr
        });
    }

    chlm::uint2 game_view_t::render_target_pixel_size() const noexcept
    {
        return _renderer.current_render_target_pixel_size();
    }
} // namespace carrot::core
