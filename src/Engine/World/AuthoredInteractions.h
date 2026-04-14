//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Scene/Scene.h"
#include "World/World.h"

#include <functional>

namespace carrot::assets {
    class asset_manager_t;
}

namespace carrot::world::authored {
    enum class interaction_kind_t : uint8_t
    {
        none = 0,
        sign,
        door,
        container
    };

    struct sign_interaction_data_t
    {
        std::string_view message_id;
    };

    struct door_interaction_data_t
    {
        std::string_view target_scene;
        std::string_view target_map;
        std::string_view target_marker;
    };

    struct container_interaction_data_t
    {
        std::string_view loot_table;
    };

    struct trigger_interaction_data_t
    {
        std::string_view trigger_id;
        std::string_view trigger_kind;
    };

    enum class interaction_outcome_kind_t : uint8_t
    {
        none = 0,
        sign,
        scene_transition,
        container
    };

    struct interaction_outcome_t
    {
        interaction_outcome_kind_t kind{ interaction_outcome_kind_t::none };
        std::string message_id;
        scene::scene_transition_request_t transition;
        world_object_id_t object_id{ 0 };
        std::string loot_table;
    };

    struct interaction_outcome_dispatch_t
    {
        std::function<void(std::string_view)> on_sign;
        std::function<void(const scene::scene_transition_request_t&)> on_scene_transition;
        std::function<void(world_object_id_t, std::string_view)> on_container;
        std::function<void(const interaction_outcome_t&)> on_unhandled;
    };

    struct scene_validation_report_t
    {
        std::vector<std::string> issues;

        [[nodiscard]] bool valid() const noexcept { return issues.empty(); }
    };

    [[nodiscard]] interaction_kind_t interaction_kind_for(const world_object_t& object) noexcept;
    [[nodiscard]] std::optional<sign_interaction_data_t> as_sign(const world_object_t& object) noexcept;
    [[nodiscard]] std::optional<door_interaction_data_t> as_door(const world_object_t& object) noexcept;
    [[nodiscard]] std::optional<container_interaction_data_t> as_container(const world_object_t& object) noexcept;
    [[nodiscard]] std::optional<trigger_interaction_data_t> as_trigger(const world_object_t& object) noexcept;
    [[nodiscard]] std::optional<interaction_outcome_t> resolve_interaction_outcome(const assets::asset_manager_t& assets,
                                                                                   const world_object_t& object);
    [[nodiscard]] bool dispatch_interaction_outcome(const interaction_outcome_t& outcome,
                                                    const interaction_outcome_dispatch_t& dispatch) noexcept;
    [[nodiscard]] std::optional<scene::scene_transition_request_t> make_scene_transition_request(
        const assets::asset_manager_t& assets,
        const world_object_t& object);
    [[nodiscard]] bool validate_scene_transition_target(const assets::asset_manager_t& assets,
                                                        const world_object_t& object) noexcept;
    [[nodiscard]] bool validate_scene_transition_targets(assets::asset_manager_t& assets,
                                                         const world_t& world) noexcept;
    [[nodiscard]] scene_validation_report_t build_scene_validation_report(assets::asset_manager_t& assets,
                                                                          const world_t& world) noexcept;
} // namespace carrot::world::authored
