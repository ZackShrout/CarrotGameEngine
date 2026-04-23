#pragma once

#include "Collision/CollisionWorld.h"
#include "World/Components/CollisionComponent.h"
#include "World/WorldObject.h"

namespace carrot::world {
    struct movement_body_t
    {
        world_object_t* object{ nullptr };

        void bind(world_object_t* bound_object) noexcept;
        [[nodiscard]] world_object_t* bound_object() noexcept { return object; }
        [[nodiscard]] const world_object_t* bound_object() const noexcept { return object; }
        [[nodiscard]] bool has_object() const noexcept { return object != nullptr; }
        [[nodiscard]] bool has_transform() const noexcept { return object && object->transform.has_value(); }
        [[nodiscard]] collision::collision_aabb_t collision_bounds_at(chlm::float2 position) const noexcept;
        [[nodiscard]] collision::collision_aabb_t current_collision_bounds() const noexcept;
    };
} // namespace carrot::world
