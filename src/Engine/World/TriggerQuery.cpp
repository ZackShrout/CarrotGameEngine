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

    void trigger_monitor_t::reset() noexcept
    {
        _active_trigger_ids.clear();
        _pending_events.clear();
    }

    void trigger_monitor_t::update(const world_object_t& actor, const world_t& world)
    {
        const trigger_overlap_changes_t changes{ update_trigger_overlaps(actor, world, _active_trigger_ids) };

        auto append_events = [this](const std::vector<const world_object_t*>& triggers,
                                    const trigger_event_phase_t phase) noexcept
        {
            for (const world_object_t* trigger : triggers)
            {
                if (!trigger)
                    continue;

                const std::optional<authored::trigger_interaction_data_t> data{ authored::as_trigger(*trigger) };
                if (!data)
                    continue;

                _pending_events.emplace_back(trigger_event_t{
                    .object_id = trigger->id,
                    .phase = phase,
                    .trigger_id = std::string{ data->trigger_id },
                    .trigger_kind = std::string{ data->trigger_kind }
                });
            }
        };

        append_events(changes.entered, trigger_event_phase_t::entered);
        append_events(changes.exited, trigger_event_phase_t::exited);
    }

    std::vector<trigger_event_t> trigger_monitor_t::consume_pending_events() noexcept
    {
        std::vector<trigger_event_t> events{ std::move(_pending_events) };
        _pending_events.clear();
        return events;
    }

    size_t trigger_monitor_t::dispatch_pending_events(const trigger_event_dispatch_t& dispatch) noexcept
    {
        size_t dispatched_count{ 0u };
        std::vector<trigger_event_t> events{ consume_pending_events() };
        for (const trigger_event_t& event : events)
        {
            bool handled{ false };
            switch (event.phase)
            {
                case trigger_event_phase_t::entered:
                    if (dispatch.on_entered)
                    {
                        dispatch.on_entered(event);
                        handled = true;
                    }
                    break;
                case trigger_event_phase_t::exited:
                    if (dispatch.on_exited)
                    {
                        dispatch.on_exited(event);
                        handled = true;
                    }
                    break;
            }

            if (dispatch.on_any)
            {
                dispatch.on_any(event);
                handled = true;
            }

            if (handled)
                ++dispatched_count;
        }

        return dispatched_count;
    }

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
            if (!object.trigger || !object.collision || object.id == actor.id)
                continue;

            if (!collision_participates_in_trigger_queries(object.collision->participation))
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
