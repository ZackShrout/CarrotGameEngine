//
// Created by Zack Shrout on 4/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "GameplayRuntimeState.h"

#include "Assets/Tilemap/TypedObjectConventions.h"

namespace sandbox {
    namespace {
        constexpr std::string_view k_container_open_flag{ "opened" };
    } // namespace

    void capture_player_runtime_state(gameplay_runtime_state_t& runtime_state,
                                      const carrot::world::player_controller_t& player_controller) noexcept
    {
        if (!player_controller.controlled_object())
            return;

        runtime_state.player_facing = player_controller.facing_direction();
    }

    void apply_runtime_state_to_player(const gameplay_runtime_state_t& runtime_state,
                                       carrot::world::player_controller_t& player_controller) noexcept
    {
        if (!runtime_state.player_facing)
            return;

        player_controller.set_facing_direction(*runtime_state.player_facing);
    }

    void mark_container_open(gameplay_runtime_state_t& runtime_state,
                             const std::string_view scene_id,
                             const carrot::world::world_object_t& container)
    {
        runtime_state.scene_flags.mark(scene_id, container, k_container_open_flag);
    }

    bool is_container_open(const gameplay_runtime_state_t& runtime_state,
                           const std::string_view scene_id,
                           const carrot::world::world_object_t& container)
    {
        return runtime_state.scene_flags.contains(scene_id, container, k_container_open_flag);
    }

    void apply_runtime_state_to_scene(const std::string_view scene_id,
                                      carrot::world::world_t& world,
                                      const gameplay_runtime_state_t& runtime_state)
    {
        (void)carrot::world::apply_scene_runtime_flag_to_matching_objects(
            scene_id,
            world,
            runtime_state.scene_flags,
            k_container_open_flag,
            [](const carrot::world::world_object_t& object)
            {
                return carrot::assets::as_typed_container(object).has_value();
            },
            [](carrot::world::world_object_t& object)
            {
                carrot::world::set_world_object_bool_property(object, "interactable", false);
            }
        );
    }
} // namespace sandbox
