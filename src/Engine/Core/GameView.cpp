//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameView.h"

#include "Renderer/Renderer.h"
#include "World/World.h"

namespace carrot::core {
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
