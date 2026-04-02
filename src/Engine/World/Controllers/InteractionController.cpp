//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "InteractionController.h"

namespace carrot::world {
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

    bool interaction_controller_t::try_interact(core::game_context_t& game)
    {
        const world_object_t* candidate{ find_candidate(game.world) };
        if (!candidate)
            return false;

        on_interact(game, *candidate);
        return true;
    }

    bool interaction_controller_t::is_interactable_candidate(const world_object_t& object) const noexcept
    {
        return object.get_bool_property("interactable").value_or(false);
    }
} // namespace carrot::world
