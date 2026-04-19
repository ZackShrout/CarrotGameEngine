//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "InteractionController.h"

namespace carrot::world {
    std::string_view to_string(const interaction_attempt_result_t result) noexcept
    {
        switch (result)
        {
            case interaction_attempt_result_t::no_actor: return "no_actor";
            case interaction_attempt_result_t::actor_missing_transform: return "actor_missing_transform";
            case interaction_attempt_result_t::no_candidate: return "no_candidate";
            case interaction_attempt_result_t::queued: return "queued";
            default: return "unknown";
        }
    }

    namespace {
        [[nodiscard]] float distance_sq(const chlm::float2 a, const chlm::float2 b) noexcept
        {
            const float dx{ a.x - b.x };
            const float dy{ a.y - b.y };
            return (dx * dx) + (dy * dy);
        }
    } // namespace

    const world_object_t* interaction_controller_t::find_candidate(const world_t& world) const noexcept
    {
        if (!_actor || !_actor->transform)
            return nullptr;

        const chlm::float2 origin{ _actor->transform->position };
        const float max_distance_sq{ _interaction_radius * _interaction_radius };

        const world_object_t* nearest{ nullptr };
        float nearest_distance_sq{ max_distance_sq };

        for (const world_object_t& object : world.objects())
        {
            if (&object == _actor || !object.transform)
                continue;

            if (!is_interactable_candidate(object))
                continue;

            const float candidate_distance_sq{ distance_sq(object.transform->position, origin) };
            if (candidate_distance_sq > max_distance_sq)
                continue;

            if (!nearest || candidate_distance_sq < nearest_distance_sq)
            {
                nearest = &object;
                nearest_distance_sq = candidate_distance_sq;
            }
        }

        return nearest;
    }

    bool interaction_controller_t::has_candidate(const world_t& world) const noexcept
    {
        return find_candidate(world) != nullptr;
    }

    std::optional<float> interaction_controller_t::candidate_distance(const world_t& world) const noexcept
    {
        if (!_actor || !_actor->transform)
            return std::nullopt;

        const world_object_t* candidate{ find_candidate(world) };
        if (!candidate || !candidate->transform)
            return std::nullopt;

        const float dx{ candidate->transform->position.x - _actor->transform->position.x };
        const float dy{ candidate->transform->position.y - _actor->transform->position.y };
        return std::sqrt((dx * dx) + (dy * dy));
    }

    interaction_attempt_result_t interaction_controller_t::attempt_interaction(core::game_context_t& game)
    {
        if (!_actor)
            return interaction_attempt_result_t::no_actor;
        if (!_actor->transform)
            return interaction_attempt_result_t::actor_missing_transform;

        const world_object_t* candidate{ find_candidate(game.world) };
        if (!candidate)
            return interaction_attempt_result_t::no_candidate;

        on_interact(game, *candidate);
        return interaction_attempt_result_t::queued;
    }

    std::optional<authored::interaction_outcome_t> interaction_controller_t::consume_pending_interaction() noexcept
    {
        std::optional<authored::interaction_outcome_t> outcome{ std::move(_pending_interaction) };
        _pending_interaction.reset();
        return outcome;
    }

    bool interaction_controller_t::dispatch_pending_interaction(const authored::interaction_outcome_dispatch_t& dispatch) noexcept
    {
        const std::optional<authored::interaction_outcome_t> outcome{ consume_pending_interaction() };
        if (!outcome)
            return false;

        return authored::dispatch_interaction_outcome(*outcome, dispatch);
    }

    bool interaction_controller_t::is_interactable_candidate(const world_object_t& object) const noexcept
    {
        return object.get_bool_property("interactable").value_or(false);
    }

    void interaction_controller_t::on_interact(core::game_context_t& game, const world_object_t& object)
    {
        const std::optional<authored::interaction_outcome_t> outcome{
            authored::resolve_interaction_outcome(game.assets, object)
        };
        if (!outcome)
        {
            LOG_CORE_INFO("Interact '{}' of type '{}'", object.name, object.type);
            return;
        }

        switch (outcome->kind)
        {
            case authored::interaction_outcome_kind_t::sign:
                LOG_CORE_INFO("Interact Sign '{}' -> message_id='{}'", object.name, outcome->message_id);
                break;
            case authored::interaction_outcome_kind_t::scene_transition:
                LOG_CORE_INFO("Interact Door '{}' -> target_scene='{}', target_marker='{}'",
                              object.name,
                              outcome->transition.scene_id,
                              outcome->transition.marker_name);
                break;
            case authored::interaction_outcome_kind_t::container:
                LOG_CORE_INFO("Interact Container '{}' -> loot_table='{}'", object.name, outcome->loot_table);
                break;
            default:
                LOG_CORE_INFO("Interact '{}' of type '{}'", object.name, object.type);
                break;
        }

        _pending_interaction = *outcome;
    }
} // namespace carrot::world
