//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Collision/CollisionWorld.h"
#include "WorldObject.h"
#include "WorldUnits.h"

#include <vector>

namespace carrot::world {
    struct collision_debug_view_t
    {
        bool show_map_collision{ false };
        bool show_object_colliders{ false };
        uint32_t map_collision_color{ 0xFF00FF00u };
        float map_outline_thickness{ 2.f };
    };

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
        [[nodiscard]] world_object_t* find_nearest_object_by_type(std::string_view type,
                                                                  const chlm::float2& origin,
                                                                  float max_distance) noexcept;
        [[nodiscard]] const world_object_t* find_nearest_object_by_type(std::string_view type,
                                                                        const chlm::float2& origin,
                                                                        float max_distance) const noexcept;
        [[nodiscard]] const std::vector<world_object_t>& objects() const noexcept { return _objects; }
        [[nodiscard]] std::vector<world_object_t>& objects() noexcept { return _objects; }
        void clear() noexcept;
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
        [[nodiscard]] const collision::collision_world_t& collision_world() const noexcept { return _collision_world; }
        [[nodiscard]] collision::collision_world_t& collision_world() noexcept { return _collision_world; }
        [[nodiscard]] const collision_debug_view_t& collision_debug_view() const noexcept { return _collision_debug_view; }
        [[nodiscard]] collision_debug_view_t& collision_debug_view() noexcept { return _collision_debug_view; }

    private:
        world_object_id_t _next_id{ 1 };
        std::vector<world_object_t> _objects;
        world_presentation_t _presentation{ };
        collision::collision_world_t _collision_world{ };
        collision_debug_view_t _collision_debug_view{ };
    };
} // namespace carrot::world
