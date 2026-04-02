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
        [[nodiscard]] world_object_t* find_first_object_by_type(std::string_view type) noexcept;
        [[nodiscard]] const world_object_t* find_first_object_by_type(std::string_view type) const noexcept;
        [[nodiscard]] std::vector<world_object_t*> find_objects_by_type(std::string_view type) noexcept;
        [[nodiscard]] std::vector<const world_object_t*> find_objects_by_type(std::string_view type) const;
        [[nodiscard]] const std::vector<world_object_t>& objects() const noexcept { return _objects; }
        [[nodiscard]] std::vector<world_object_t>& objects() noexcept { return _objects; }
        void update(float delta_time) noexcept;
        void set_presentation_origin_px(const chlm::float2 origin) noexcept { _presentation.origin_px = origin; }
        [[nodiscard]] const chlm::float2& presentation_origin_px() const noexcept { return _presentation.origin_px; }
        void set_presentation_pixels_per_unit(const float pixels_per_unit) noexcept
        {
            _presentation.set_pixels_per_unit(pixels_per_unit);
        }
        [[nodiscard]] float presentation_pixels_per_unit() const noexcept { return _presentation.pixels_per_unit; }
        [[nodiscard]] const world_presentation_t& presentation() const noexcept { return _presentation; }
        [[nodiscard]] world_presentation_t& presentation() noexcept { return _presentation; }

    private:
        world_object_id_t _next_id{ 1 };
        std::vector<world_object_t> _objects;
        world_presentation_t _presentation{ };
    };
} // namespace carrot::world
