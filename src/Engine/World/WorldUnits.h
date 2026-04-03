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

    struct world_presentation_t
    {
        chlm::float2 origin_px{ 0.f, 0.f };
        float pixels_per_unit{ world_units_t::default_render_pixels_per_unit };

        void set_pixels_per_unit(const float value) noexcept
        {
            pixels_per_unit = value > 0.f
                ? value
                : world_units_t::default_render_pixels_per_unit;
        }

        [[nodiscard]] chlm::float2 world_position_to_pixels(const chlm::float2 world_position) const noexcept
        {
            return origin_px + world_units_t::world_size_to_pixels(world_position, pixels_per_unit);
        }

        [[nodiscard]] chlm::float2 pixel_position_to_world(const chlm::float2 pixel_position) const noexcept
        {
            return world_units_t::pixel_size_to_world(pixel_position - origin_px, pixels_per_unit);
        }

        [[nodiscard]] chlm::float2 world_size_to_pixels(const chlm::float2 world_size) const noexcept
        {
            return world_units_t::world_size_to_pixels(world_size, pixels_per_unit);
        }
    };
} // namespace carrot::world
