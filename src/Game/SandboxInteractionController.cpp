//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "SandboxInteractionController.h"

#include "WorldInteractionHelpers.h"

namespace sandbox {
    std::optional<scene_transition_request_t> sandbox_interaction_controller_t::consume_pending_transition() noexcept
    {
        std::optional<scene_transition_request_t> request{ std::move(_pending_transition) };
        _pending_transition.reset();
        return request;
    }

    std::optional<opened_container_request_t> sandbox_interaction_controller_t::consume_pending_opened_container() noexcept
    {
        std::optional<opened_container_request_t> request{ std::move(_pending_opened_container) };
        _pending_opened_container.reset();
        return request;
    }

    void sandbox_interaction_controller_t::on_interact(carrot::core::game_context_t& game,
                                                       const carrot::world::world_object_t& object)
    {
        if (const std::optional<sign_interaction_data_t> sign{ as_sign(object) })
        {
            LOG_CORE_INFO("Interact Sign '{}' -> message_id='{}'", object.name, sign->message_id);
            return;
        }

        if (const std::optional<door_interaction_data_t> door{ as_door(object) })
        {
            const std::optional<scene_transition_request_t> request{
                make_scene_transition_request(game.assets, object)
            };
            if (!request)
            {
                LOG_CORE_WARN("Interact Door '{}' could not resolve a destination scene", object.name);
                return;
            }

            _pending_transition = *request;
            LOG_CORE_INFO("Interact Door '{}' -> target_scene='{}', target_marker='{}'",
                          object.name,
                          request->scene_id,
                          request->marker_name);
            return;
        }

        if (const std::optional<container_interaction_data_t> container{ as_container(object) })
        {
            _pending_opened_container = opened_container_request_t{
                .object_id = object.id
            };
            LOG_CORE_INFO("Interact Container '{}' -> loot_table='{}'", object.name, container->loot_table);
            return;
        }

        LOG_CORE_INFO("Interact '{}' of type '{}'", object.name, object.type);
    }
} // namespace sandbox
