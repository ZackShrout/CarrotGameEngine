//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::world {
    struct world_units_t
    {
        static constexpr float world_unit_in_meters{ 1.0f };
        static constexpr float default_pixels_per_unit{ 16.0f };
        static constexpr float default_scene_render_scale{ 4.0f };
        static constexpr float default_render_pixels_per_unit{
            default_pixels_per_unit * default_scene_render_scale
        };

        [[nodiscard]] static constexpr float pixels_to_world(const float pixels,
                                                             const float pixels_per_unit = default_pixels_per_unit) noexcept
        {
            return pixels / pixels_per_unit;
        }

        [[nodiscard]] static constexpr chlm::float2 pixel_size_to_world(
            const chlm::float2 pixel_size,
            const float pixels_per_unit = default_pixels_per_unit) noexcept
        {
            return {
                pixels_to_world(pixel_size.x, pixels_per_unit),
                pixels_to_world(pixel_size.y, pixels_per_unit)
            };
        }

        [[nodiscard]] static constexpr float world_to_pixels(const float world_units,
                                                             const float pixels_per_unit = default_pixels_per_unit) noexcept
        {
            return world_units * pixels_per_unit;
        }

        [[nodiscard]] static constexpr chlm::float2 world_size_to_pixels(
            const chlm::float2 world_size,
            const float pixels_per_unit = default_pixels_per_unit) noexcept
        {
            return {
                world_to_pixels(world_size.x, pixels_per_unit),
                world_to_pixels(world_size.y, pixels_per_unit)
            };
        }
    };
} // namespace carrot::world
