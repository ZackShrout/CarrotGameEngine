//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "InteractionController.h"
#include "World/AuthoredInteractions.h"
#include "World/WorldUnits.h"

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

        [[nodiscard]] collision::collision_aabb_t world_object_collision_bounds(const world_object_t& object) noexcept
        {
            const collision_component_t collision{
                object.collision.value_or(collision_component_t{
                    .half_extents = { 0.f, 0.f },
                    .offset = { 0.f, 0.f }
                })
            };
            return collision::collision_aabb_t::from_center_extents(
                object.transform->position + collision.offset,
                collision.half_extents);
        }

        [[nodiscard]] std::optional<collision::collision_aabb_t> authored_rectangle_bounds(
            const world_object_t& object) noexcept
        {
            if (!object.transform || !object.authored_geometry)
                return std::nullopt;
            if (object.authored_geometry->kind != assets::tilemap_object_t::geometry_kind_t::rectangle)
                return std::nullopt;
            if (object.authored_geometry->size_source_px.x <= 0.f || object.authored_geometry->size_source_px.y <= 0.f)
                return std::nullopt;

            return collision::collision_aabb_t::from_min_size(
                object.transform->position,
                chlm::float2{
                    world_units_t::pixels_to_world(object.authored_geometry->size_source_px.x),
                    world_units_t::pixels_to_world(object.authored_geometry->size_source_px.y)
                });
        }

        [[nodiscard]] std::optional<float> interaction_distance_sq(const world_object_t& actor,
                                                                   const world_object_t& candidate) noexcept
        {
            if (!actor.transform || !candidate.transform)
                return std::nullopt;

            if (const auto candidate_rect{ authored_rectangle_bounds(candidate) })
            {
                if (actor.collision)
                {
                    const collision::collision_aabb_t actor_bounds{ world_object_collision_bounds(actor) };
                    if (collision::collision_aabb_overlaps(actor_bounds, *candidate_rect))
                        return 0.f;
                    return std::nullopt;
                }

                if (collision::collision_aabb_contains_point(*candidate_rect, actor.transform->position))
                    return 0.f;
                return std::nullopt;
            }

            return distance_sq(candidate.transform->position, actor.transform->position);
        }
    } // namespace

    const world_object_t* interaction_controller_t::find_candidate(const world_t& world) const noexcept
    {
        if (!_actor || !_actor->transform)
            return nullptr;

        const float max_distance_sq{ _interaction_radius * _interaction_radius };

        const world_object_t* nearest{ nullptr };
        float nearest_distance_sq{ max_distance_sq };

        for (const world_object_t& object : world.objects())
        {
            if (&object == _actor || !object.transform)
                continue;

            if (!is_interactable_candidate(object))
                continue;

            const std::optional<float> candidate_distance_sq{ interaction_distance_sq(*_actor, object) };
            if (!candidate_distance_sq.has_value())
                continue;

            if (*candidate_distance_sq > max_distance_sq)
                continue;

            if (!nearest || *candidate_distance_sq < nearest_distance_sq)
            {
                nearest = &object;
                nearest_distance_sq = *candidate_distance_sq;
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
        if (!object.get_bool_property("interactable").value_or(false))
            return false;

        return authored::interaction_kind_for(object) != authored::interaction_kind_t::none;
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
