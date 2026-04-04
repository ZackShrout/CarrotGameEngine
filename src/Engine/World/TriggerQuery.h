//
// Created by Codex on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "WorldObject.h"

#include <unordered_set>
#include <vector>

namespace carrot::world {
    class world_t;

    struct trigger_overlap_changes_t
    {
        std::vector<const world_object_t*> entered;
        std::vector<const world_object_t*> staying;
        std::vector<const world_object_t*> exited;
    };

    [[nodiscard]] trigger_overlap_changes_t update_trigger_overlaps(
        const world_object_t& actor,
        const world_t& world,
        std::unordered_set<world_object_id_t>& active_trigger_ids);
} // namespace carrot::world
