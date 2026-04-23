//
// Created by Zack Shrout on 4/23/26.
//

#include "Core/Pch.h"

#include "MovementBody.h"

namespace carrot::world {
    namespace {
        [[nodiscard]] collision_component_t default_collision_component() noexcept
        {
            return collision_component_t{
                .half_extents = { 0.3f, 0.2f },
                .offset = { 0.f, -0.2f },
                .debug_display = std::nullopt
            };
        }
    } // namespace

    void movement_body_t::bind(world_object_t* bound_object) noexcept
    {
        object = bound_object;
        if (object && !object->collision)
            object->collision = default_collision_component();
    }

    collision::collision_aabb_t movement_body_t::collision_bounds_at(const chlm::float2 position) const noexcept
    {
        const collision_component_t collision{
            object && object->collision ? *object->collision : default_collision_component()
        };
        return collision::collision_aabb_t::from_center_extents(position + collision.offset, collision.half_extents);
    }

    collision::collision_aabb_t movement_body_t::current_collision_bounds() const noexcept
    {
        if (!object || !object->transform)
            return collision::collision_aabb_t{ };

        return collision_bounds_at(object->transform->position);
    }
} // namespace carrot::world
