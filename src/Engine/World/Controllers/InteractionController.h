//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/GameContext.h"
#include "World/AuthoredInteractions.h"
#include "World/World.h"

#include <string_view>

namespace carrot::world {
    enum class interaction_attempt_result_t : uint8_t
    {
        no_actor = 0,
        actor_missing_transform,
        no_candidate,
        queued
    };

    [[nodiscard]] std::string_view to_string(interaction_attempt_result_t result) noexcept;

    class interaction_controller_t
    {
    public:
        virtual ~interaction_controller_t() = default;

        void set_actor(world_object_t* actor) noexcept { _actor = actor; }
        [[nodiscard]] world_object_t* actor() noexcept { return _actor; }
        [[nodiscard]] const world_object_t* actor() const noexcept { return _actor; }
        [[nodiscard]] bool has_actor() const noexcept { return _actor != nullptr; }

        void set_interaction_radius(float radius) noexcept { _interaction_radius = radius; }
        [[nodiscard]] float interaction_radius() const noexcept { return _interaction_radius; }

        [[nodiscard]] const world_object_t* find_candidate(const world_t& world) const noexcept;
        [[nodiscard]] bool has_candidate(const world_t& world) const noexcept;
        [[nodiscard]] std::optional<float> candidate_distance(const world_t& world) const noexcept;
        [[nodiscard]] interaction_attempt_result_t attempt_interaction(core::game_context_t& game);
        [[nodiscard]] std::optional<authored::interaction_outcome_t> consume_pending_interaction() noexcept;
        [[nodiscard]] bool dispatch_pending_interaction(const authored::interaction_outcome_dispatch_t& dispatch) noexcept;

    protected:
        [[nodiscard]] virtual bool is_interactable_candidate(const world_object_t& object) const noexcept;
        virtual void on_interact(core::game_context_t& game, const world_object_t& object);

    private:
        world_object_t* _actor{ nullptr };
        float _interaction_radius{ 3.0f };
        std::optional<authored::interaction_outcome_t> _pending_interaction;
    };
} // namespace carrot::world
