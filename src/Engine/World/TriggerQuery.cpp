//
// Created by zshrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "TriggerQuery.h"

#include "World.h"

namespace carrot::world {
    namespace {
        [[nodiscard]] std::optional<collision::collision_aabb_t> collision_bounds_for(const world_object_t& object) noexcept
        {
            if (!object.transform || !object.collision)
                return std::nullopt;

            return collision::collision_aabb_t::from_center_extents(
                object.transform->position + object.collision->offset,
                object.collision->half_extents
            );
        }
    } // namespace

    trigger_overlap_changes_t update_trigger_overlaps(const world_object_t& actor,
                                                      const world_t& world,
                                                      std::unordered_set<world_object_id_t>& active_trigger_ids)
    {
        trigger_overlap_changes_t changes;
        const std::optional<collision::collision_aabb_t> actor_bounds{ collision_bounds_for(actor) };
        if (!actor_bounds)
            return changes;

        std::unordered_set<world_object_id_t> next_active_ids;

        for (const world_object_t& object : world.objects())
        {
            if (!object.trigger || object.id == actor.id)
                continue;

            const std::optional<collision::collision_aabb_t> trigger_bounds{ collision_bounds_for(object) };
            if (!trigger_bounds || !collision::collision_aabb_overlaps(*actor_bounds, *trigger_bounds))
                continue;

            next_active_ids.insert(object.id);

            if (active_trigger_ids.contains(object.id))
                changes.staying.push_back(&object);
            else
                changes.entered.push_back(&object);
        }

        for (const world_object_t& object : world.objects())
        {
            if (!object.trigger || !active_trigger_ids.contains(object.id) || next_active_ids.contains(object.id))
                continue;

            changes.exited.push_back(&object);
        }

        active_trigger_ids = std::move(next_active_ids);
        return changes;
    }
} // namespace carrot::world
