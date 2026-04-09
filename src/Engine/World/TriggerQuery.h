//
// Created by zshrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "World/AuthoredInteractions.h"
#include "WorldObject.h"

#include <unordered_set>
#include <vector>

namespace carrot::world {
    class world_t;

    enum class trigger_event_phase_t : uint8_t
    {
        entered = 0,
        exited
    };

    struct trigger_event_t
    {
        world_object_id_t object_id{ 0 };
        trigger_event_phase_t phase{ trigger_event_phase_t::entered };
        std::string trigger_id;
        std::string trigger_kind;
    };

    struct trigger_overlap_changes_t
    {
        std::vector<const world_object_t*> entered;
        std::vector<const world_object_t*> staying;
        std::vector<const world_object_t*> exited;
    };

    class trigger_monitor_t
    {
    public:
        void reset() noexcept;
        void update(const world_object_t& actor, const world_t& world);
        [[nodiscard]] std::vector<trigger_event_t> consume_pending_events() noexcept;

    private:
        std::unordered_set<world_object_id_t> _active_trigger_ids;
        std::vector<trigger_event_t> _pending_events;
    };

    [[nodiscard]] trigger_overlap_changes_t update_trigger_overlaps(
        const world_object_t& actor,
        const world_t& world,
        std::unordered_set<world_object_id_t>& active_trigger_ids);
} // namespace carrot::world
