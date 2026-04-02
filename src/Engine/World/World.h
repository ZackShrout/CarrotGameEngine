//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "WorldUnits.h"
#include "WorldObject.h"

#include <vector>

namespace carrot::world {
    class world_t
    {
    public:
        [[nodiscard]] world_object_t& create_object();
        [[nodiscard]] world_object_t* find_object_by_name(std::string_view name) noexcept;
        [[nodiscard]] const world_object_t* find_object_by_name(std::string_view name) const noexcept;
        [[nodiscard]] const std::vector<world_object_t>& objects() const noexcept { return _objects; }
        [[nodiscard]] std::vector<world_object_t>& objects() noexcept { return _objects; }
        void update(float delta_time) noexcept;
        void set_render_origin_px(const chlm::float2 origin) noexcept { _render_origin_px = origin; }
        [[nodiscard]] const chlm::float2& render_origin_px() const noexcept { return _render_origin_px; }
        void set_render_pixels_per_unit(const float pixels_per_unit) noexcept
        {
            _render_pixels_per_unit = pixels_per_unit > 0.f
                ? pixels_per_unit
                : world_units_t::default_render_pixels_per_unit;
        }
        [[nodiscard]] float render_pixels_per_unit() const noexcept { return _render_pixels_per_unit; }

    private:
        world_object_id_t _next_id{ 1 };
        std::vector<world_object_t> _objects;
        chlm::float2 _render_origin_px{ 0.f, 0.f };
        float _render_pixels_per_unit{ world_units_t::default_render_pixels_per_unit };
    };
} // namespace carrot::world
