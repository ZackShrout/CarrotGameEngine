//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SandboxInteractionController.h"

#include "WorldInteractionHelpers.h"

namespace sandbox {
    void sandbox_interaction_controller_t::on_interact([[maybe_unused]] carrot::core::game_context_t& game,
                                                       const carrot::world::world_object_t& object)
    {
        if (const std::optional<sign_interaction_data_t> sign{ as_sign(object) })
        {
            LOG_CORE_INFO("Interact Sign '{}' -> message_id='{}'", object.name, sign->message_id);
            return;
        }

        if (const std::optional<door_interaction_data_t> door{ as_door(object) })
        {
            LOG_CORE_INFO("Interact Door '{}' -> target_map='{}', target_marker='{}'",
                          object.name,
                          door->target_map,
                          door->target_marker);
            return;
        }

        if (const std::optional<chest_interaction_data_t> chest{ as_chest(object) })
        {
            LOG_CORE_INFO("Interact Chest '{}' -> loot_table='{}'", object.name, chest->loot_table);
            return;
        }

        LOG_CORE_INFO("Interact '{}' of type '{}'", object.name, object.type);
    }
} // namespace sandbox
